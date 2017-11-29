//
// Copyright (c) 2014-2016 THUNDERBEAST GAMES LLC
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include <Poco/MD5Engine.h>

#include <Atomic/IO/Log.h>
#include <Atomic/IO/File.h>
#include <Atomic/IO/FileSystem.h>
#include "AssetDatabase.h"
#include "AssetImporter.h"
#include "AssetCacheManager.h"
#include "AssetCacheEvents.h"

namespace ToolCore
{

AssetImporter::AssetImporter(Context* context, Asset *asset) : Object(context),
    asset_(asset),
    requiresCacheFile_(false),
    requiresDotAssetMd5_(false)
{
    SetDefaults();

    SubscribeToEvent(E_ASSETCACHE_FETCH_SUCCESS, ATOMIC_HANDLER(AssetImporter, HandleAssetCacheFetchSuccess));
    SubscribeToEvent(E_ASSETCACHE_FETCH_FAIL, ATOMIC_HANDLER(AssetImporter, HandleAssetCacheFetchFail));
}

AssetImporter::~AssetImporter()
{

}

void AssetImporter::SetDefaults()
{

}

bool AssetImporter::LoadSettings(JSONValue& root)
{
    LoadSettingsInternal(root);
    return true;
}

bool AssetImporter::LoadSettingsInternal(JSONValue& jsonRoot)
{
    return true;
}

bool AssetImporter::SaveSettings(JSONValue& root)
{
    SaveSettingsInternal(root);

    return true;
}

bool AssetImporter::SaveSettingsInternal(JSONValue& jsonRoot)
{
    return true;
}

bool AssetImporter::Move(const String& newPath)
{
    FileSystem* fs = GetSubsystem<FileSystem>();

    if (newPath == asset_->path_)
        return false;

    String oldPath = asset_->path_;
    String oldName = asset_->name_;

    String pathName, newName, ext;

    SplitPath(newPath, pathName, newName, ext);

    // rename asset first, ahead of the filesystem watcher, so the assetdatabase doesn't see a new asset
    asset_->name_ = newName;
    asset_->path_ = newPath;

    // first rename the .asset file
    if (!fs->Rename(oldPath + ".asset", newPath + ".asset"))
    {
        asset_->name_ = oldName;
        asset_->path_ = oldPath;

        ATOMIC_LOGERRORF("Unable to rename asset: %s to %s", GetNativePath(oldPath + ".asset").CString(), GetNativePath(newPath + ".asset").CString());
        return false;
    }

    // now rename the asset file itself
    if (!fs->Rename(oldPath, newPath))
    {
        asset_->name_ = oldName;
        asset_->path_ = oldPath;

        // restore .asset
        fs->Rename(newPath + ".asset", oldPath + ".asset");

        ATOMIC_LOGERRORF("Unable to rename: %s to %s", GetNativePath(oldPath).CString(), GetNativePath(newPath).CString());
        return false;
    }

    return true;
}

void AssetImporter::ClearCacheFiles()
{
    FileSystem* fs = GetSubsystem<FileSystem>();
    AssetDatabase* db = GetSubsystem<AssetDatabase>();
    String cachePath = db->GetCachePath();

    fs->Delete(GetDotMD5FilePath());

    Vector<String> filesToRequest;
    GetRequiredCacheFiles(filesToRequest);

    for (int i = 0; i < filesToRequest.Size(); i++)
    {
        String cacheFile = cachePath + filesToRequest[i];
        fs->Delete(cacheFile);
    }
}

bool AssetImporter::Rename(const String& newName)
{
    String pathName, fileName, ext;

    SplitPath(asset_->path_, pathName, fileName, ext);

    String newPath = pathName + newName + ext;

    FileSystem* fs = GetSubsystem<FileSystem>();

    if (fs->FileExists(newPath) || fs->DirExists(newPath))
        return false;

    return Move(newPath);

}


bool AssetImporter::Import()
{
    // Import may be called directly by reimporting the asset
    if (!requiresCacheFile_)
        return true;

    cacheFetchFilesPending_.Clear();
    cacheFetchFilesFailed_.Clear();

    Vector<String> filesToRequest;
    GetRequiredCacheFiles(filesToRequest);

    // if we got here but we're in an importer that does require a cache file, assert. Should be at least one file in the list
    assert(filesToRequest.Size() > 0);

    for (int i = 0; i < filesToRequest.Size(); i++)
    {
        ATOMIC_LOGDEBUGF("AssetImporter::Import - cache file requested[%d] - %s", i, filesToRequest[i].CString());
    }

    if (md5_.Empty())
        md5_ = GenerateFileMD5(asset_->path_);
    if (requiresDotAssetMd5_ && dotAssetMd5_.Empty())
        dotAssetMd5_ = GenerateFileMD5(asset_->GetDotAssetFilename());

    // try to fetch the cache files from the asset cache manager
    TryFetchCacheFiles(filesToRequest);

    return true;
}

void AssetImporter::TryFetchCacheFiles(Vector<String>& files)
{
    assert(md5_.Length());

    AssetDatabase* db = GetSubsystem<AssetDatabase>();

    SharedPtr<AssetCacheManager> cacheManager = db->GetCacheManager();

    assert(cacheManager.NotNull());

    // must add all the files to the pending list first, before calling fetch on each one individually.
    cacheFetchFilesPending_.Push(files);

    Vector<String>::ConstIterator itr = files.Begin();
    while (itr != files.End())
    {
        cacheManager->FetchCacheFile(*itr, md5_);
        itr++;
    }
}

bool AssetImporter::GenerateCacheFiles()
{
    assert(requiresCacheFile_);

    return WriteMD5File();
}

void AssetImporter::HandleAssetCacheFetchSuccess(StringHash eventType, VariantMap& eventData)
{
    if (!requiresCacheFile_)
        return;

    using namespace AssetCacheFetchSuccess;

    const String& fileName = eventData[P_CACHEFILENAME].GetString();
    const String& md5 = eventData[P_CACHEMD5].GetString();

    if (md5 == md5_ &&
        cacheFetchFilesPending_.Contains(fileName))
    {
        cacheFetchFilesPending_.Remove(fileName);

        if (cacheFetchFilesPending_.Empty())
        {
            if (cacheFetchFilesFailed_.Empty())
            {
                asset_->OnImportComplete();
            }
            else if (GenerateCacheFiles())
            {
                OnCacheFilesGenerated(cacheFetchFilesFailed_);
                asset_->OnImportComplete();
            }
        }
    }
}

void AssetImporter::HandleAssetCacheFetchFail(StringHash eventType, VariantMap& eventData)
{
    if (!requiresCacheFile_)
        return;

    using namespace AssetCacheFetchFail;

    const String& fileName = eventData[P_CACHEFILENAME].GetString();
    const String& md5 = eventData[P_CACHEMD5].GetString();

    if (md5 == md5_ &&
        cacheFetchFilesPending_.Contains(fileName))
    {
        cacheFetchFilesPending_.Remove(fileName);
        cacheFetchFilesFailed_.Push(fileName);

        // we'll only try to generate the files once we're fetched all the files we can from the manager,
        // so that we generate the cache files only once.
        if (cacheFetchFilesPending_.Empty() && GenerateCacheFiles())
        {
            OnCacheFilesGenerated(cacheFetchFilesFailed_);
            asset_->OnImportComplete();
        }
    }
}

void AssetImporter::OnCacheFilesGenerated(Vector<String>& files)
{
    AssetDatabase* db = GetSubsystem<AssetDatabase>();
    SharedPtr<AssetCacheManager> cacheManager = db->GetCacheManager();

    Vector<String>::ConstIterator itr = files.Begin();

    while (itr != files.End())
    {
        cacheManager->OnFileAddedToCache(*itr, md5_);
        itr++;
    }
}

bool AssetImporter::CheckCacheFilesUpToDate()
{
    md5_ = GenerateFileMD5(asset_->path_);
    if (requiresDotAssetMd5_)
        dotAssetMd5_ = GenerateFileMD5(asset_->GetDotAssetFilename());

    if (md5_ + dotAssetMd5_ != ReadMD5File())
    {
        ClearCacheFiles();
        return false;
    }

    FileSystem* fs = GetSubsystem<FileSystem>();
    AssetDatabase* db = GetSubsystem<AssetDatabase>();
    String cachePath = db->GetCachePath();

    Vector<String> filesToRequest;
    GetRequiredCacheFiles(filesToRequest);

    for (int i = 0; i < filesToRequest.Size(); i++)
    {
        String cacheFile = cachePath + filesToRequest[i];

        if (!fs->FileExists(cacheFile))
            return false;
    }

    return true;
}

String AssetImporter::GetDotMD5FilePath()
{
    AssetDatabase* db = GetSubsystem<AssetDatabase>();
    String cachePath = db->GetCachePath();
    return cachePath + asset_->guid_ + ".md5";
}

String AssetImporter::ReadMD5File()
{
    FileSystem* fs = GetSubsystem<FileSystem>();

    String md5FilePath = GetDotMD5FilePath();
    if (!fs->Exists(md5FilePath))
        return String::EMPTY;

    SharedPtr<File> file(new File(context_, md5FilePath, FILE_READ));
    return file->ReadString();
}

bool AssetImporter::WriteMD5File()
{
    // Ensure md5 is current
    md5_ = GenerateFileMD5(asset_->path_);
    if (requiresDotAssetMd5_)
        dotAssetMd5_ = GenerateFileMD5(asset_->GetDotAssetFilename());

    String md5FilePath = GetDotMD5FilePath();
    FileSystem* fs = GetSubsystem<FileSystem>();
    if (fs->Exists(md5FilePath))
        fs->Delete(md5FilePath);

    SharedPtr<File> file(new File(context_, md5FilePath, FILE_WRITE));
    return file->WriteString(md5_ + dotAssetMd5_);
}

String AssetImporter::GenerateDeserializerMD5(Deserializer& stream)
{
    Poco::MD5Engine md5;

    // add stream data into the hash
    unsigned streamSize = stream.GetSize();
    SharedArrayPtr<unsigned char> streamData(new unsigned char[streamSize]);

    unsigned sizeRead = stream.Read(streamData, streamSize);

    assert(sizeRead == streamSize);

    md5.update(&streamData[0], streamSize * sizeof(unsigned char));

    // generate a guid
    return Poco::MD5Engine::digestToHex(md5.digest()).c_str();
}

String AssetImporter::GenerateFileMD5(String& path)
{
    assert(path.Length());

    if (asset_->isFolder_)
    {
        return Poco::MD5Engine::digestToHex(Poco::MD5Engine().digest()).c_str();
    }

    File assetFile(context_, path);

    return GenerateDeserializerMD5(assetFile);
}

void AssetImporter::UpdateMD5()
{
    md5_ = GenerateFileMD5(asset_->path_);
    if (requiresDotAssetMd5_)
        dotAssetMd5_ = GenerateFileMD5(asset_->GetDotAssetFilename());
}

bool AssetImporter::CheckDotAssetMD5(Deserializer& deserializer)
{
    UpdateMD5();

    deserializer.Seek(0);
    String streamMD5 = GenerateDeserializerMD5(deserializer);

    return streamMD5 == dotAssetMd5_;
}
}
