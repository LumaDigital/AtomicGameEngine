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

#pragma once

#include <Atomic/Core/Object.h>
#include <Atomic/Resource/JSONFile.h>
#include <Atomic/Scene/Node.h>

using namespace Atomic;

namespace ToolCore
{

class Asset;

/// deals with .asset files
class AssetImporter : public Object
{
    friend class Asset;

    ATOMIC_OBJECT(AssetImporter, Object);

public:
    /// Construct.
    AssetImporter(Context* context, Asset* asset);
    virtual ~AssetImporter();

    // load .asset
    bool LoadSettings(JSONValue& root);
    // save .asset
    bool SaveSettings(JSONValue& root);

    virtual void SetDefaults();

    virtual bool Preload() { return true; }

    Asset* GetAsset() { return asset_; }

    virtual Resource* GetResource(const String& typeName = String::EMPTY) { return 0; }

    bool GetRequiresCacheFile() const { return requiresCacheFile_; }

    // Whether changes to .asset files affect generated cache files
    bool GetRequiresDotAsset() const { return requiresDotAssetMd5_; }

    void ClearCacheFiles();

    /// Instantiate a node from the asset
    virtual Node* InstantiateNode(Node* parent, const String& name) { return 0; }

    virtual bool Rename(const String& newName);
    virtual bool Move(const String& newPath);

protected:

    // Get a mapping of the assets path to cache file representations, by type
    virtual void GetAssetCacheMap(HashMap<String, String>& assetMap) {}

    virtual bool Import();

    WeakPtr<Asset> asset_;
    bool requiresCacheFile_;
    bool requiresDotAssetMd5_;

    // LUMA Begin
    void UpdateMD5();
    // LUMA End

    virtual void GetRequiredCacheFiles(Vector<String>& files) {}
    virtual void TryFetchCacheFiles(Vector<String>& files);
    virtual bool GenerateCacheFiles();
    virtual void OnCacheFilesGenerated(Vector<String>& files);
    virtual bool CheckCacheFilesUpToDate();

    virtual bool LoadSettingsInternal(JSONValue& jsonRoot);
    virtual bool SaveSettingsInternal(JSONValue& jsonRoot);

    virtual void HandleAssetCacheFetchSuccess(StringHash eventType, VariantMap& eventData);
    virtual void HandleAssetCacheFetchFail(StringHash eventType, VariantMap& eventData);

    Vector<String> cacheFetchFilesPending_;
    Vector<String> cacheFetchFilesFailed_;

private:
    /// get the path to the file containing the MD5 in last import
    String GetDotMD5FilePath();
    /// Read the MD5 of the asset on last import
    String ReadMD5File();
    /// Write the MD5 of the asset to file
    bool WriteMD5File();
    /// Generate an MD5 hash from a deserializer
    static String GenerateDeserializerMD5(Deserializer& deserializer);
    /// Generate an MD5 hash from a file at path
    String GenerateFileMD5(String& path);
    /// Compares the md5 of the current .asset file with 
    /// that corresponding to the provided stream
    bool CheckDotAssetMD5(Deserializer& deserializer);

    String md5_;
    String dotAssetMd5_;
};

}
