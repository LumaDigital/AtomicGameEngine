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
#include <Atomic/Math/Random.h>
#include <Atomic/Core/CoreEvents.h>

#include <Atomic/Resource/ResourceEvents.h>
#include <Atomic/Resource/ResourceCache.h>

#include "../Import/ImportConfig.h"
#include "../ToolEvents.h"
#include "../ToolSystem.h"
#include "../Project/Project.h"
#include "../Project/ProjectEvents.h"
#include "AssetEvents.h"
#include "AssetDatabase.h"
#include "AssetCacheConfig.h"
#include "AssetCacheManagerLocal.h"
#include "AssetCacheManagerNetwork.h"



namespace ToolCore
{

AssetDatabase::AssetDatabase(Context* context) : 
    Object(context), 
    assetScanDepth_(0),
    cacheEnabled_(true),
    assetCacheMapDirty_(false),
    doingImport_(false),
    doingProjectLoad_(false),
    cacheManager_(nullptr)
{
    SubscribeToEvent(E_LOADFAILED, ATOMIC_HANDLER(AssetDatabase, HandleResourceLoadFailed));
    SubscribeToEvent(E_PROJECTBASELOADED, ATOMIC_HANDLER(AssetDatabase, HandleProjectBaseLoaded));
    SubscribeToEvent(E_PROJECTUNLOADED, ATOMIC_HANDLER(AssetDatabase, HandleProjectUnloaded));
    SubscribeToEvent(E_BEGINFRAME, ATOMIC_HANDLER(AssetDatabase, HandleBeginFrame));    
}

AssetDatabase::~AssetDatabase()
{

}

String AssetDatabase::GetCachePath()
{
    if (project_.Null())
        return String::EMPTY;

    return project_->GetProjectPath() + "Cache/";

}

String AssetDatabase::GenerateAssetGUID()
{

    Time* time = GetSubsystem<Time>();

    while (true)
    {
        Poco::MD5Engine md5;
        PODVector<unsigned> data;

        for (unsigned i = 0; i < 16; i++)
        {
            data.Push(time->GetTimeSinceEpoch() + Rand());
        }

        md5.update(&data[0], data.Size() * sizeof(unsigned));

        String guid = Poco::MD5Engine::digestToHex(md5.digest()).c_str();

        if (!usedGUID_.Contains(guid))
        {
            RegisterGUID(guid);
            return guid;
        }
    }

    assert(0);
    return "";
}

void AssetDatabase::RegisterGUID(const String& guid)
{
    if (usedGUID_.Contains(guid))
    {
        assert(0);
    }

    usedGUID_.Push(guid);
}

void AssetDatabase::ReadAssetCacheConfig()
{
    AssetCacheConfig::Clear();

    ToolSystem* tsystem = GetSubsystem<ToolSystem>();
    Project* project = tsystem->GetProject();

    String projectPath = project->GetProjectPath();

    String filename = projectPath + "Settings/AssetCache.json";

    FileSystem* fileSystem = GetSubsystem<FileSystem>();
    if (!fileSystem->FileExists(filename))
        return;

    AssetCacheConfig::LoadFromFile(context_, filename);
}

void AssetDatabase::ReadImportConfig()
{
    ImportConfig::Clear();

    ToolSystem* tsystem = GetSubsystem<ToolSystem>();
    Project* project = tsystem->GetProject();

    String projectPath = project->GetProjectPath();

    String filename = projectPath + "Settings/Import.json";

    FileSystem* fileSystem = GetSubsystem<FileSystem>();
    if (!fileSystem->FileExists(filename))
        return;

    ImportConfig::LoadFromFile(context_, filename);
}

void AssetDatabase::Import(const String& path)
{
    FileSystem* fs = GetSubsystem<FileSystem>();

    // nothing for now
    if (fs->DirExists(path))
        return;
}

Asset* AssetDatabase::GetAssetByCachePath(const String& cachePath)
{
    List<SharedPtr<Asset>>::ConstIterator itr = assets_.Begin();

    // This is the GUID
    String cacheFilename = GetFileName(cachePath).ToLower();

    while (itr != assets_.End())
    {
        if ((*itr)->GetGUID().ToLower() == cacheFilename)
            return *itr;

        itr++;
    }

    return 0;

}

Asset* AssetDatabase::GetAssetByGUID(const String& guid)
{
    List<SharedPtr<Asset>>::ConstIterator itr = assets_.Begin();

    while (itr != assets_.End())
    {
        if (guid == (*itr)->GetGUID())
            return *itr;

        itr++;
    }

    return 0;

}

Asset* AssetDatabase::GetAssetByPath(const String& path)
{
    List<SharedPtr<Asset>>::ConstIterator itr = assets_.Begin();

    while (itr != assets_.End())
    {
        SharedPtr<Asset> asset = *itr;
        if (path == asset->GetPath())
            return *itr;

        itr++;
    }

    return 0;

}

Asset* AssetDatabase::GetAssetByResourcePath(const String& path)
{
    if (project_)
    {
        return GetAssetByPath(project_->GetResourcePath() + path);
    }
    else
    {
        return GetAssetByPath(path);
    }
}

void AssetDatabase::PruneOrphanedDotAssetFiles()
{

    if (project_.Null())
    {
        ATOMIC_LOGDEBUG("AssetDatabase::PruneOrphanedDotAssetFiles - called without project loaded");
        return;
    }

    FileSystem* fs = GetSubsystem<FileSystem>();

    const String& resourcePath = project_->GetResourcePath();

    Vector<String> allResults;

    fs->ScanDir(allResults, resourcePath, "*.asset", SCAN_FILES, true);

    for (unsigned i = 0; i < allResults.Size(); i++)
    {
        String dotAssetFilename = resourcePath + allResults[i];
        String assetFilename = ReplaceExtension(dotAssetFilename, "");

        // remove orphaned asset files
        if (!fs->FileExists(assetFilename) && !fs->DirExists(assetFilename))
        {

            ATOMIC_LOGINFOF("Removing orphaned asset file: %s", dotAssetFilename.CString());
            fs->Delete(dotAssetFilename);
        }

    }
}

String AssetDatabase::GetDotAssetFilename(const String& path)
{
    FileSystem* fs = GetSubsystem<FileSystem>();

    String assetFilename = path + ".asset";

    if (fs->DirExists(path)) {

        assetFilename = RemoveTrailingSlash(path) + ".asset";
    }

    return assetFilename;

}

void AssetDatabase::AddAsset(SharedPtr<Asset>& asset, bool newAsset)
{
    assert(asset->GetGUID().Length());
    assert(!GetAssetByGUID(asset->GetGUID()));

    assets_.Push(asset);

    // only send the event now if the asset isn't dirty (ie isn't in the process of being imported)
    // if it is dirty, the event will be sent when the import completes and the asset is ready to be used.
    if (asset->GetState() == AssetState::CLEAN)
    {
        VariantMap& eventData = GetEventDataMap();

        if (newAsset)
        {        
            eventData[AssetNew::P_GUID] = asset->GetGUID();
            SendEvent(E_ASSETNEW, eventData);
        }

        
        eventData[ResourceAdded::P_GUID] = asset->GetGUID();
        SendEvent(E_RESOURCEADDED, eventData);
    }
}

void AssetDatabase::DeleteAsset(Asset* asset)
{
    SharedPtr<Asset> assetPtr(asset);

    List<SharedPtr<Asset>>::Iterator itr = assets_.Find(assetPtr);

    if (itr == assets_.End())
        return;

    assets_.Erase(itr);

    asset->Remove();
}

bool AssetDatabase::ImportDirtyAssets()
{
    PODVector<Asset*> assets;
    GetDirtyAssets(assets);

    for (unsigned i = 0; i < assets.Size(); i++)
    {
        assets[i]->BeginImport();
    }
        
    bool startedAnyImports = (assets.Size() != 0);

    // note - since imports can take time, even if no files were set to import in this call, some files can still be importing that were set to import previously.
    // Hence, we xor the flag.
    doingImport_ |= startedAnyImports;
    assetCacheMapDirty_ |= startedAnyImports;

    return startedAnyImports;
}

void AssetDatabase::PreloadAssets()
{
    List<SharedPtr<Asset>>::ConstIterator itr = assets_.Begin();

    while (itr != assets_.End())
    {
        (*itr)->Preload();
        itr++;
    }

}

void AssetDatabase::GetAllAssetPaths(Vector<String>& assetPaths)
{
    FileSystem* fs = GetSubsystem<FileSystem>();
    const String& resourcePath = project_->GetResourcePath();

    Vector<String> allResults;

    fs->ScanDir(allResults, resourcePath, "", SCAN_FILES | SCAN_DIRS, true);

    assetPaths.Push(RemoveTrailingSlash(resourcePath));

    for (unsigned i = 0; i < allResults.Size(); i++)
    {
        allResults[i] = resourcePath + allResults[i];

        const String& path = allResults[i];

        if (path.StartsWith(".") || path.EndsWith("."))
            continue;

        String ext = GetExtension(path);

        if (ext == ".asset")
            continue;

        assetPaths.Push(path);
    }

}

void AssetDatabase::UpdateAssetCacheMap()
{
    FileSystem* fileSystem = GetSubsystem<FileSystem>();

    if (project_.Null())
        return;

    bool gen = assetCacheMapDirty_;
    assetCacheMapDirty_ = false;

    String cachepath = project_->GetProjectPath() + "Cache/__atomic_ResourceCacheMap.json";

    if (!gen && !fileSystem->FileExists(cachepath))
        gen = true;

    if (!gen)
        return;

    List<SharedPtr<Asset>>::ConstIterator itr = assets_.Begin();

    HashMap<String, String> assetMap;
    JSONValue jAssetMap;

    while (itr != assets_.End())
    {
        assetMap.Clear();
        (*itr)->GetAssetCacheMap(assetMap);

        HashMap<String, String>::ConstIterator amitr = assetMap.Begin();

        while (amitr != assetMap.End())
        {
            jAssetMap.Set(amitr->first_, amitr->second_);
            amitr++;
        }

        itr++;
    }

    SharedPtr<File> file(new File(context_, cachepath, FILE_WRITE));
    if (!file->IsOpen())
    {
        ATOMIC_LOGERRORF("Unable to update ResourceCacheMap: %s", cachepath.CString());
        return;
    }


    SharedPtr<JSONFile> jsonFile(new JSONFile(context_));
    jsonFile->GetRoot().Set("assetMap", jAssetMap);

    jsonFile->Save(*file);


}





// 1) Remove all orphaned dot asset files
// 2) Find all asset files and add them to our asset list (if they are valid assets and aren't in the list already).
// 3) Create or init all assets
// 4) Preload all assets.
// 5) Import any assets that are dirty.
void AssetDatabase::Scan()
{   
    if (!assetScanDepth_)
    {
        SendEvent(E_ASSETSCANBEGIN);
    }

    assetScanDepth_++;

    PruneOrphanedDotAssetFiles();

    Vector<String> assetPaths;
    GetAllAssetPaths(assetPaths);

    // LUMA BEGIN
    Vector<String> allAssetGuids;
    // LUMA END

    FileSystem* fs = GetSubsystem<FileSystem>();
    for (unsigned i = 0; i < assetPaths.Size(); i++)
    {
        const String& path = assetPaths[i];

        String dotAssetFilename = GetDotAssetFilename(path);

        if (!fs->FileExists(dotAssetFilename))
        {
            // new asset
            SharedPtr<Asset> asset(new Asset(context_));

            if (asset->SetPath(path))
                AddAsset(asset);
        }
        else
        {
            SharedPtr<File> file(new File(context_, dotAssetFilename));
            SharedPtr<JSONFile> json(new JSONFile(context_));
            json->Load(*file);
            file->Close();

            JSONValue& root = json->GetRoot();

            assert(root.Get("version").GetInt() == ASSET_VERSION);

            String guid = root.Get("guid").GetString();

            if (!GetAssetByGUID(guid))
            {
                SharedPtr<Asset> asset(new Asset(context_));
                asset->SetPath(path);
                AddAsset(asset);
            }

            // LUMA BEGIN
            allAssetGuids.Push(guid);
            // LUMA END
        }
    }

    PreloadAssets();

    // LUMA BEGIN
    ClearDeletedCacheFiles(allAssetGuids);
    // LUMA END

    if (ImportDirtyAssets())
        Scan();

    assetScanDepth_--;

    if (!assetScanDepth_)
    {   
        SendEvent(E_ASSETSCANEND);        
    }
}

void AssetDatabase::GetFolderAssets(String folder, PODVector<Asset*>& assets) const
{
    if (project_.Null())
        return;

    List<SharedPtr<Asset>>::ConstIterator itr = assets_.Begin();

    if (!folder.Length())
    {
        folder = project_->GetResourcePath();
    }

    folder = AddTrailingSlash(folder);

    while (itr != assets_.End())
    {
        String path = GetPath((*itr)->GetPath());

        if (path == folder)
            assets.Push(*itr);

        itr++;
    }

}

void AssetDatabase::GetAssetsByImporterType(StringHash type, const String &resourceType, PODVector<Asset*>& assets) const
{
    assets.Clear();

    List<SharedPtr<Asset>>::ConstIterator itr = assets_.Begin();

    while (itr != assets_.End())
    {
        Asset* asset = *itr;

        if (asset->GetImporterType() == type)
            assets.Push(asset);

        itr++;
    }

}

void AssetDatabase::GetDirtyAssets(PODVector<Asset*>& assets)
{
    assets.Clear();

    List<SharedPtr<Asset>>::ConstIterator itr = assets_.Begin();

    while (itr != assets_.End())
    {
        Asset* asset = *itr;
        if ((*itr)->GetState() == AssetState::DIRTY)
            assets.Push(*itr);
        itr++;
    }
}

void AssetDatabase::HandleProjectBaseLoaded(StringHash eventType, VariantMap& eventData)
{
    project_ = GetSubsystem<ToolSystem>()->GetProject();

    ReadImportConfig();
    ReadAssetCacheConfig();
    
    VariantMap assetCacheMap;
    AssetCacheConfig::ApplyConfig(assetCacheMap);

    if (assetCacheMap["UseServer"].GetBool())
    {
        cacheManager_ = SharedPtr<AssetCacheManager>(new AssetCacheManagerNetwork(context_));
    }
    else
    {
        cacheManager_ = SharedPtr<AssetCacheManager>(new AssetCacheManagerLocal(context_));
    }


    if (cacheEnabled_)
    {
        InitCache();
    }

    SubscribeToEvent(E_FILECHANGED, ATOMIC_HANDLER(AssetDatabase, HandleFileChanged));
}

void AssetDatabase::HandleProjectUnloaded(StringHash eventType, VariantMap& eventData)
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    cache->RemoveResourceDir(GetCachePath());
    assets_.Clear();
    usedGUID_.Clear();
    assetImportErrorTimes_.Clear();
    project_ = 0;

    UnsubscribeFromEvent(E_FILECHANGED);
}

void AssetDatabase::HandleResourceLoadFailed(StringHash eventType, VariantMap& eventData)
{

    if (project_.Null())
        return;

    String path = eventData[LoadFailed::P_RESOURCENAME].GetString();

    Asset* asset = GetAssetByPath(path);

    if (!asset)
        asset = GetAssetByPath(project_->GetResourcePath() + path);

    if (!asset)
        return;

    Time* time = GetSubsystem<Time>();

    unsigned ctime = time->GetSystemTime();

    // if less than 5 seconds since last report, stifle report
    if (assetImportErrorTimes_.Contains(asset->guid_))
        if (ctime - assetImportErrorTimes_[asset->guid_] < 5000)
            return;

    assetImportErrorTimes_[asset->guid_] = ctime;

    VariantMap evData;
    evData[AssetImportError::P_PATH] = asset->path_;
    evData[AssetImportError::P_GUID] = asset->guid_;
    evData[AssetImportError::P_ERROR] = ToString("Asset %s Failed to Load", asset->path_.CString());
    SendEvent(E_ASSETIMPORTERROR, evData);

}

void AssetDatabase::HandleFileChanged(StringHash eventType, VariantMap& eventData)
{
    using namespace FileChanged;
    const String& fullPath = eventData[P_FILENAME].GetString();

    FileSystem* fs = GetSubsystem<FileSystem>();

    String pathName, fileName, ext;

    SplitPath(fullPath, pathName, fileName, ext);

    // ignore changes in the Cache resource dir
    if (fullPath == GetCachePath() || pathName.StartsWith(GetCachePath()))
        return;

    // don't care about directories and asset file changes
    if (fs->DirExists(fullPath) || ext == ".asset")
        return;

    Asset* asset = GetAssetByPath(fullPath);

    if (!asset && fs->FileExists(fullPath))
    {
        Scan();
        return;
    }

    if (asset)
    {
        if(!fs->Exists(fullPath))
        {
            DeleteAsset(asset);
        }
        else
        {
            if (asset->CacheNeedsUpdate())
            {
                asset->SetState(AssetState::DIRTY);
                Scan();
            }
        }
    }
}

String AssetDatabase::GetResourceImporterName(const String& resourceTypeName)
{
    // TODO: have resource type register themselves
    if (resourceTypeToImporterType_.Empty())
    {
        resourceTypeToImporterType_["Sound"] = "AudioImporter";
        resourceTypeToImporterType_["Model"] = "ModelImporter";
        resourceTypeToImporterType_["Material"] = "MaterialImporter";
        resourceTypeToImporterType_["Texture2D"] = "TextureImporter";
        resourceTypeToImporterType_["Sprite2D"] = "TextureImporter";
        resourceTypeToImporterType_["Image"] = "TextureImporter";
        resourceTypeToImporterType_["AnimatedSprite2D"] = "SpriterImporter";
        resourceTypeToImporterType_["JSComponentFile"] = "JavascriptImporter";
        resourceTypeToImporterType_["JSONFile"] = "JSONImporter";
        resourceTypeToImporterType_["ParticleEffect2D"] = "PEXImporter";
        resourceTypeToImporterType_["ParticleEffect"] = "ParticleEffectImporter";

        resourceTypeToImporterType_["Animation"] = "ModelImporter";

        resourceTypeToImporterType_["CSComponentAssembly"] = "NETAssemblyImporter";
        resourceTypeToImporterType_["TmxFile2D"] = "TMXImporter";

    }

    if (!resourceTypeToImporterType_.Contains(resourceTypeName))
        return String::EMPTY;

    return resourceTypeToImporterType_[resourceTypeName];

}

void AssetDatabase::ReimportAllAssets()
{
    List<SharedPtr<Asset>>::ConstIterator itr = assets_.Begin();

    while (itr != assets_.End())
    {
        (*itr)->SetState(AssetState::DIRTY);
        itr++;
    }

    Scan();
}

void AssetDatabase::ReimportAllAssetsInDirectory(const String& directoryPath)
{
    List<SharedPtr<Asset>>::ConstIterator itr = assets_.Begin();

    while (itr != assets_.End())
    {
        if ((*itr)->GetPath().StartsWith(directoryPath))
        {
            (*itr)->SetState(AssetState::DIRTY);
        }
        itr++;
    }

    Scan();

}

bool AssetDatabase::InitCache()
{
    FileSystem* fs = GetSubsystem<FileSystem>();

    if (!fs->DirExists(GetCachePath()))
        fs->CreateDir(GetCachePath());

    ResourceCache* cache = GetSubsystem<ResourceCache>();
    cache->AddResourceDir(GetCachePath());

    // must set this before the scan!
    doingProjectLoad_ = true;

    ATOMIC_LOGDEBUG("AssetDatabase - scanning assets on project load.");

    Scan();

    if (!doingImport_ && doingProjectLoad_)
    {
        CompleteProjectAssetsLoad();
    }

    return true;
}

bool AssetDatabase::CleanCache()
{
    FileSystem* fileSystem = GetSubsystem<FileSystem>();

    String cachePath = GetCachePath();

    if (fileSystem->DirExists(cachePath))
    {
        ATOMIC_LOGINFOF("Cleaning cache directory %s", cachePath.CString());

        fileSystem->RemoveDir(cachePath, true);

        if (fileSystem->DirExists(cachePath))
        {
            ATOMIC_LOGERRORF("Unable to remove cache directory %s", cachePath.CString());
            return false;
        }
    }

    fileSystem->CreateDir(cachePath);

    if (!fileSystem->DirExists(cachePath))
    {
        ATOMIC_LOGERRORF("Unable to create cache directory %s", cachePath.CString());
        return false;
    }

    return true;

}

bool AssetDatabase::GenerateCache(bool clean)
{
    ATOMIC_LOGINFO("Generating cache... hold on");

    if (clean)
    {
        if (!CleanCache())
            return false;
    }
    else
    {
        FileSystem* fileSystem = GetSubsystem<FileSystem>();
        String cachePath = GetCachePath();
        if (!fileSystem->DirExists(cachePath))
        {
            fileSystem->CreateDir(cachePath);
        }
    }

    // Required to ensure completion event is sent
    doingProjectLoad_ = true;

    ReimportAllAssets();

    ATOMIC_LOGINFO("Cache generated");

    return true;

}

void AssetDatabase::SetCacheEnabled(bool cacheEnabled)
{
    cacheEnabled_ = cacheEnabled;
}

void AssetDatabase::HandleBeginFrame(StringHash eventType, VariantMap& eventData)
{
    using namespace BeginFrame;

    Update(eventData[P_TIMESTEP].GetFloat());
}

void AssetDatabase::Update(float timeStep)
{
    if (!doingImport_)
    {
        return;
    }

    List<SharedPtr<Asset>>::ConstIterator itr = assets_.Begin();

    bool allAssetsClean = true;

    while (itr != assets_.End())
    {
        SharedPtr<Asset> asset = (*itr);

        if (asset->GetState() == AssetState::IMPORT_COMPLETE)
        {
            asset->SetState(AssetState::CLEAN);

            // notify the editor UI etc
            VariantMap eventData;
            eventData[ResourceAdded::P_GUID] = asset->GetGUID();
            SendEvent(E_RESOURCEADDED, eventData);
        }
        else if (
            asset->GetState() == AssetState::IMPORTING ||
            asset->GetState() == AssetState::DIRTY
            )
        {
            allAssetsClean = false;
        }

        itr++;
    }

    if (doingProjectLoad_ && allAssetsClean)
    {
        CompleteProjectAssetsLoad();
    }

    if (allAssetsClean)
    {
        doingImport_ = false;
    }
}

void AssetDatabase::CompleteProjectAssetsLoad()
{
    ATOMIC_LOGDEBUG("AssetDatabase - All project assets finished loading.");

    // gotta do this here when all the assets are finished loading.
    UpdateAssetCacheMap();

    doingProjectLoad_ = false;
    VariantMap data;
    SendEvent(E_PROJECTASSETSLOADED, data);

}

// LUMA BEGIN
void AssetDatabase::ClearDeletedCacheFiles(Vector<String> assetGuids)
{
    const String atomicPrefix = "__atomic_";

    FileSystem* fs = GetSubsystem<FileSystem>();
    Vector<String> cacheFiles;
    fs->ScanDir(cacheFiles, GetCachePath(), "", SCAN_FILES, true);

    for (int i = 0; i < cacheFiles.Size(); i++)
    {
        String& cacheFile = cacheFiles[i];
        if (cacheFile.StartsWith(atomicPrefix))
        {
            continue;
        }

        bool found = false;

        for (int j = 0; j < assetGuids.Size(); j++)
        {
            if (cacheFile.Contains(assetGuids[j]))
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            // Delete from local cache
            String cacheFileName = GetCachePath() + cacheFiles[i];
            fs->Delete(cacheFileName);
        }
    }
}
// LUMA END

}





