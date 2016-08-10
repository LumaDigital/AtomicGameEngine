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
#include <Atomic/Resource/Resource.h>
#include <Atomic/Scene/Node.h>

#include "AssetImporter.h"

using namespace Atomic;

namespace ToolCore
{

#define ASSET_VERSION 1


enum AssetState
{
    DIRTY,
    IMPORTING,
    IMPORT_COMPLETE, // this state is similar to 'CLEAN', it exists so that the AssetDatabase can check when an asset has finished loading and fire events based on that before marking an asset 'CLEAN'.
    IMPORT_FAILED,
    CLEAN,
};

class Asset : public Object
{
    friend class AssetDatabase;
    friend class AssetImporter;

    ATOMIC_OBJECT(Asset, Object);

public:

    /// Construct.
    Asset(Context* context);
    virtual ~Asset();
    
    void BeginImport();
    bool Preload();

    // the .fbx, .png, etc path, attempts to load .asset, creates missing .asset
    bool SetPath(const String& path);

    void Remove();

    const String& GetGUID() const { return guid_; }
    const String& GetName() const { return name_; }

    // Get absolute path to asset file
    const String& GetPath() const { return path_; }

    String GetExtension() const;
    /// Get the path relative to project
    String GetRelativePath();
    String GetCachePath() const;

    Resource* GetResource(const String& typeName = String::EMPTY);

    const StringHash GetImporterType() { return importer_.Null() ? String::EMPTY : importer_->GetType(); }
    const String& GetImporterTypeName() { return importer_.Null() ? String::EMPTY : importer_->GetTypeName(); }

    AssetImporter* GetImporter() { return importer_; }

    void OnImportComplete();
    void OnImportError(const String& message);

    Asset* GetParent();

    void SetDirty() { SetState(AssetState::DIRTY); }
    void SetState(AssetState newState) { assetState_ = newState; }
    AssetState GetState() { return assetState_; }

    /// Get the last timestamp as seen by the AssetDatabase
    unsigned GetFileTimestamp() { return fileTimestamp_; }

    /// Sets the time stamp to the asset files current time
    void UpdateFileTimestamp();

    // get the .asset filename
    String GetDotAssetFilename();

    /// Rename the asset, which depending on the asset type may be nontrivial
    bool Rename(const String& newName);

    /// Move the asset, which depending on the asset type may be nontrivial
    bool Move(const String& newPath);

    bool IsFolder() const { return isFolder_; }

    // load .asset
    bool Load();
    // save .asset
    bool Save();

    /// Instantiate a node from the asset
    Node* InstantiateNode(Node* parent, const String& name);

    // Get a mapping of the assets path to cache file representations, by type
    void GetAssetCacheMap(HashMap<String, String>& assetMap) { if (importer_.NotNull()) importer_->GetAssetCacheMap(assetMap); }


private:

    bool CreateImporter();

    bool CacheNeedsUpdate();

    String guid_;

    // can change
    String path_;
    String name_;

    AssetState assetState_;
    bool isFolder_;

    // the current timestamp of the asset as seen by the asset database
    // used to catch when the asset needs to be marked dirty when notified
    // that the file has changed (the resource system will send a changed file
    // event when the resource is first added)
    unsigned fileTimestamp_;

    SharedPtr<JSONFile> json_;
    SharedPtr<AssetImporter> importer_;
};

}
