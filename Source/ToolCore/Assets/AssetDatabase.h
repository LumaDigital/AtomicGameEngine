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
#include <Atomic/Container/List.h>
#include "AssetCacheManager.h"
#include "Asset.h"

using namespace Atomic;

namespace ToolCore
{

class Project;

class AssetDatabase : public Object
{
    ATOMIC_OBJECT(AssetDatabase, Object);

public:
    /// Construct.
    AssetDatabase(Context* context);
    virtual ~AssetDatabase();

    Asset* GetAssetByGUID(const String& guid);
    Asset* GetAssetByPath(const String& path);
    Asset* GetAssetByResourcePath(const String& path);
    Asset* GetAssetByCachePath(const String& cachePath);

    String GenerateAssetGUID();
    void RegisterGUID(const String& guid);

    String GetCachePath();

    /// Get whether the asset cache is enabled
    bool GetCacheEnabled() const { return cacheEnabled_; }

    /// Set whether the asset cache is enabled 
    void SetCacheEnabled(bool cacheEnabled);

    /// Cleans the asset Cache folder by removing and recreating it
    bool CleanCache();

    /// Regenerates the asset cache, clean removes the Cache folder before generating
	/// waitForImports blocks until all assets have been fully imported
    bool GenerateCache(bool clean = true);

    void DeleteAsset(Asset* asset);

    void Scan();

    void ReimportAllAssets();
    void ReimportAllAssetsInDirectory(const String& directoryPath);

    void GetFolderAssets(String folder, PODVector<Asset*>& assets) const;

    String GetResourceImporterName(const String& resourceTypeName);

    void GetAssetsByImporterType(StringHash type, const String& resourceType, PODVector<Asset*>& assets) const;

    void GetDirtyAssets(PODVector<Asset*>& assets);

    String GetDotAssetFilename(const String& path);

    const SharedPtr<AssetCacheManager>& GetCacheManager() { return cacheManager_;  }

private:

    void Update(float timeStep);

    void HandleProjectBaseLoaded(StringHash eventType, VariantMap& eventData);
    void HandleProjectUnloaded(StringHash eventType, VariantMap& eventData);
    void HandleFileChanged(StringHash eventType, VariantMap& eventData);
    void HandleResourceLoadFailed(StringHash eventType, VariantMap& eventData);
    void HandleBeginFrame(StringHash eventType, VariantMap& eventData);

    void AddAsset(SharedPtr<Asset>& asset, bool newAsset = false);

    void PruneOrphanedDotAssetFiles();
    // LUMA BEGIN
    void ClearDeletedCacheFiles(Vector<String> assetGuids);
    // LUMA END

    void ReadAssetCacheConfig();

    void ReadImportConfig();
    void Import(const String& path);

    void GetAllAssetPaths(Vector<String>& assetPaths);

    void CompleteProjectAssetsLoad();

    bool ImportDirtyAssets();
    void PreloadAssets();

    // internal method that initializes project asset cache
    bool InitCache();

    // Update mapping of asset paths to cache file representations, by type
    void UpdateAssetCacheMap();

    SharedPtr<Project> project_;
    List<SharedPtr<Asset>> assets_;

    HashMap<StringHash, String> resourceTypeToImporterType_;

    /// Hash value of times, so we don't spam import errors
    HashMap<StringHash, unsigned> assetImportErrorTimes_;

    unsigned assetScanDepth_;

    Vector<String> usedGUID_;

    // Whether the asset cache map needs updatig
    bool assetCacheMapDirty_;
    
    bool doingProjectLoad_;
    bool doingImport_;

    bool cacheEnabled_;

    SharedPtr<AssetCacheManager> cacheManager_;
};

}
