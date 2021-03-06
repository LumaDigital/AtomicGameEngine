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

#include <Atomic/IO/Log.h>
#include <Atomic/IO/File.h>
#include <Atomic/IO/VectorBuffer.h>
#include <Atomic/IO/FileSystem.h>

#include "../ToolSystem.h"
#include "../Project/Project.h"

#include "AssetDatabase.h"
#include "AudioImporter.h"
#include "ModelImporter.h"
#include "FolderImporter.h"
#include "SceneImporter.h"
#include "MaterialImporter.h"
#include "TextureImporter.h"
#include "PrefabImporter.h"
#include "JavascriptImporter.h"
#include "JSONImporter.h"
#include "SpriterImporter.h"
#include "TMXImporter.h"
#include "PEXImporter.h"
#include "TextImporter.h"
#include "TypeScriptImporter.h"
#include "ParticleEffectImporter.h"
#include "CSharpImporter.h"
#include "NETAssemblyImporter.h"

#include "AssetEvents.h"
#include "Asset.h"

namespace ToolCore
{

Asset::Asset(Context* context) :
    Object(context),
    assetState_(AssetState::CLEAN),
    isFolder_(false),
    fileTimestamp_(0xffffffff)
{

}

Asset::~Asset()
{

}

void Asset::UpdateFileTimestamp()
{
    FileSystem* fs = GetSubsystem<FileSystem>();

    if (fs->FileExists(path_))
    {
        fileTimestamp_ = fs->GetLastModifiedTime(path_);
    }
}

Asset* Asset::GetParent()
{
    AssetDatabase* db = GetSubsystem<AssetDatabase>();

    String pathName;
    String fileName;
    String ext;

    SplitPath(path_, pathName, fileName, ext);

    return db->GetAssetByPath(RemoveTrailingSlash(pathName));

}

String Asset::GetRelativePath()
{
    Project* project =GetSubsystem<ToolSystem>()->GetProject();

    String path = path_;

    path.Replace(project->GetResourcePath(), "", false);

    return path;

}

bool Asset::CacheNeedsUpdate()
{
    if (importer_.Null())
    {
        return true;
    }

    if (!importer_->GetRequiresCacheFile())
    {
        return false;
    }

    return !importer_->CheckCacheFilesUpToDate();

}

void Asset::BeginImport()
{
    if (importer_.Null())
        return;

    assetState_ = AssetState::IMPORTING;

    // refresh .asset settings if import depends on them
    if (importer_->GetRequiresDotAsset())
    {
        LoadDotAssetJson();
        importer_->LoadSettings(json_->GetRoot());
        json_ = 0;
    }

    //start the importer importing, it'll notify us once it's succeeded or hit an error.
    importer_->Import();
}

bool Asset::Preload()
{
    if (importer_.Null())
        return true;

    // disabled preload for now, as this is on a background thread and causing init problems
    return true;
    //return importer_->Preload();
}

void Asset::OnImportComplete()
{
    Save();
    assetState_ = AssetState::IMPORT_COMPLETE;
}

void Asset::OnImportError(const String& message)
{
    assetState_ = AssetState::IMPORT_FAILED;

    VariantMap eventData;
    eventData[AssetImportError::P_PATH] = path_;
    eventData[AssetImportError::P_GUID] = guid_;
    eventData[AssetImportError::P_ERROR] = message;

    SendEvent(E_ASSETIMPORTERROR, eventData);
}

bool Asset::LoadDotAssetJson()
{
    FileSystem* fs = GetSubsystem<FileSystem>();

    String assetFilename = GetDotAssetFilename();

    SharedPtr<File> file(new File(context_, assetFilename));
    json_ = new JSONFile(context_);
    json_->Load(*file);
    file->Close();

    return true;
}


// load .asset
bool Asset::Load()
{
    if (!LoadDotAssetJson())
        return false;

    JSONValue& root = json_->GetRoot();

    assert(root.Get("version").GetInt() == ASSET_VERSION);

    guid_ = root.Get("guid").GetString();

    AssetDatabase* db = GetSubsystem<AssetDatabase>();
    db->RegisterGUID(guid_);

    if (importer_.NotNull())
        importer_->LoadSettings(root);

    if (CacheNeedsUpdate())
    {
        assetState_ = AssetState::DIRTY;
    }

    json_ = 0;

    return true;
}

// save .asset
bool Asset::Save()
{
    String assetFilename = GetDotAssetFilename();

    json_ = new JSONFile(context_);

    JSONValue& root = json_->GetRoot();

    root.Set("version", JSONValue(ASSET_VERSION));

    //for the convenience of being able to open the .asset file and check what the guid is, we'll save it out.
    //but we don't read it in the load function, instead we generate it from the file to make sure it hasn't changed since we saved it last.
    root.Set("guid", JSONValue(guid_));

    // handle importer settings
    if (importer_.NotNull())
    {
        importer_->SaveSettings(root);

        if (importer_->GetRequiresDotAsset())
        {
            // prevent unnecessary detection of .asset file changes
            // when that would lead to reloading of the asset and repeated resaving of the .asset
            VectorBuffer buffer;
            json_->Save(buffer);

            if (!importer_->CheckDotAssetMD5(buffer))
            {
                SharedPtr<File> file(new File(context_, assetFilename, FILE_WRITE));
                file->Write(buffer.GetData(), buffer.GetSize());
                file->Close();
                importer_->WriteMD5File();
            }
        }
        else
        {
            SharedPtr<File> file(new File(context_, assetFilename, FILE_WRITE));
            json_->Save(*file);
            file->Close();
            importer_->WriteMD5File();
        }
    }

    json_ = 0;

    return true;
}

String Asset::GetDotAssetFilename()
{
    assert(path_.Length());

    FileSystem* fs = GetSubsystem<FileSystem>();

    String assetFilename = path_ + ".asset";

    if (fs->DirExists(path_)) {

        assetFilename = RemoveTrailingSlash(path_) + ".asset";

    }

    return assetFilename;

}

bool Asset::CreateImporter()
{
    assert(importer_.Null());

    FileSystem* fs = GetSubsystem<FileSystem>();

    if (fs->DirExists(path_))
    {
        name_ = GetFileName(RemoveTrailingSlash(path_));
        isFolder_ = true;
        importer_ = new FolderImporter(context_, this);
    }
    else
    {
        String ext = Atomic::GetExtension(path_);

        name_ = GetFileName(path_);

        Vector<String> textureFormats;
        textureFormats.Push(".jpg");
        textureFormats.Push(".png");
        textureFormats.Push(".tga");
        textureFormats.Push(".dds");

        // todo, externalize recognizers
        if (ext == ".fbx" || ext == ".blend" || ext == ".dae" || ext == ".mdl")
        {
            importer_ = new ModelImporter(context_, this);
        }
        else if (ext == ".ogg" || ext == ".wav")
        {
            importer_ = new AudioImporter(context_, this);
        }
        else if (ext == ".prefab")
        {
            importer_ = new PrefabImporter(context_, this);
        }
        else if (ext == ".js")
        {
            importer_ = new JavascriptImporter(context_, this);
        }
        else if (ext == ".ts")
        {
            importer_ = new TypeScriptImporter(context_, this);
        }
        else if (ext == ".json")
        {
            importer_ = new JSONImporter(context_, this);
        }
        else if (ext == ".scene")
        {
            importer_ = new SceneImporter(context_, this);
        }
        else if (ext == ".material")
        {
            importer_ = new MaterialImporter(context_, this);
        }
        else if (ext == ".scml")
        {
            importer_ = new SpriterImporter(context_, this);
        }
        else if (ext == ".tmx")
        {
            importer_ = new TMXImporter(context_, this);
        }
        else if (ext == ".pex")
        {
            importer_ = new PEXImporter(context_, this);
        }
        else if (ext == ".peffect")
        {
            importer_ = new ParticleEffectImporter(context_, this);
        }
        else if (ext == ".txt" || ext == ".xml" || ext == ".hlsl" || ext == ".glsl")
        {
            importer_ = new TextImporter(context_, this);
        }
        else if (ext == ".dll")
        {
            importer_ = new NETAssemblyImporter(context_, this);
        }
        else if (ext == ".cs")
        {
            importer_ = new CSharpImporter(context_, this);
        }
        else if (textureFormats.Contains(ext))
        {
            importer_ = new TextureImporter(context_, this);
        }
        else
        {
            importer_ = new AssetImporter(context_, this);
        }

    }

    if (importer_.Null())
        return false;

    return true;

}

String Asset::GetCachePath() const
{
    AssetDatabase* db = GetSubsystem<AssetDatabase>();
    String cachePath = db->GetCachePath();
    cachePath += guid_;
    return cachePath;
}

String Asset::GetExtension() const
{
    return Atomic::GetExtension(path_);
}

bool Asset::SetPath(const String& path)
{
    assert(!guid_.Length());
    assert(!path_.Length());

    // need to update path, not set, which should only be done on first import
    assert(importer_.Null());

    FileSystem* fs = GetSubsystem<FileSystem>();
    AssetDatabase* db = GetSubsystem<AssetDatabase>();

    path_ = path;

    // reset asset state to clean, code below will update it to dirty if appropriate.
    assetState_ = AssetState::CLEAN;

    // create importer based on path
    if (!CreateImporter())
        return false;

    String assetFilename = GetDotAssetFilename();

    if (fs->FileExists(assetFilename))
    {
        // load the json, will check if cache up to date and set dirty if not. Todo: handle fail
        Load();
    }
    else
    {
        //If the dot file doesn't exist, create the GUID & dot file then mark it as needing an import.
        guid_ = db->GenerateAssetGUID();
        Save();

        if (CacheNeedsUpdate())
        {
            assetState_ = AssetState::DIRTY;
        }
    }

    // TODO: handle failed
    return true;

}

void Asset::Remove()
{
    const String& resourcePath = GetPath();

    FileSystem* fs = GetSubsystem<FileSystem>();

    if (fs->DirExists(resourcePath))
    {
        fs->RemoveDir(resourcePath, true);
    }
    else if (fs->FileExists(resourcePath))
    {
        fs->Delete(resourcePath);
    }

    String dotAsset = resourcePath + ".asset";

    if (fs->FileExists(dotAsset))
    {
        fs->Delete(dotAsset);
    }

    if (importer_)
    {
        importer_->ClearCacheFiles();
    }

    VariantMap eventData;
    eventData[ResourceRemoved::P_GUID] = GetGUID();
    // LUMA BEGIN
    eventData[ResourceRemoved::P_NAME] = GetName();
    // LUMA END
    SendEvent(E_RESOURCEREMOVED, eventData);
}

bool Asset::Move(const String& newPath)
{
    if (importer_.Null())
        return false;

    String oldPath = path_;

    bool result = importer_->Move(newPath);

    // load the json, will check if cache up to date and set dirty if not.
    Load();

    if (result)
    {
        VariantMap eventData;
        eventData[AssetMoved::P_ASSET] = this;
        eventData[AssetMoved::P_OLDPATH] = oldPath;
        SendEvent(E_ASSETMOVED, eventData);
    }

    return result;

}

bool Asset::Rename(const String& newName)
{
    if (importer_.Null())
        return false;

    bool result = importer_->Rename(newName);

    // load the json, will check if cache up to date and set dirty if not.
    Load();

    if (result)
    {
        VariantMap eventData;
        eventData[AssetRenamed::P_ASSET] = this;
        SendEvent(E_ASSETRENAMED, eventData);
    }

    return result;
}

Resource* Asset::GetResource(const String &typeName)
{
    if (importer_)
        return importer_->GetResource(typeName);

    return 0;
}

Node* Asset::InstantiateNode(Node* parent, const String& name)
{
    if (!parent)
        return 0;

    if (importer_)
        return importer_->InstantiateNode(parent, name);

    return 0;

}

}
