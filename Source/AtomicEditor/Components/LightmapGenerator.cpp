//
// Copyright (c) 2008-2015 the Urho3D project.
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

#include <Atomic/Core/Context.h>
#include <Atomic/Scene/Scene.h>
#include <Atomic/Scene/Node.h>
#include <Atomic/Graphics/Material.h>
#include <Atomic/Graphics/Technique.h>
#include <Atomic/Graphics/Octree.h>
#include <Atomic/IO/Log.h>
#include <Atomic/IO/FileSystem.h>
#include <Atomic/Atomic3D/LMStaticModel.h>
#include <Atomic/Resource/ResourceCache.h>

#include <ToolCore/ToolSystem.h>
#include <ToolCore/Project/Project.h>

#include "../Editors/SceneEditor3D/SceneEditor3D.h"
#include "../Editors/SceneEditor3D/SceneEditor3DEvents.h"
#include "../Utils/StaticModelLightmapGenerator.h"
#include "../Utils/ImageAtlasGenerator.h"

#include "LightmapGenerator.h"

using namespace Atomic;
using namespace AtomicEditor;

LightmapGenerator::LightmapGenerator(Context* context) :
    EditorComponent(context),
    pixelsPerUnit_(64),
    blurImage_(false),
    bleedRadius_(5)
{
}

LightmapGenerator::~LightmapGenerator()
{
}

void LightmapGenerator::RegisterObject(Context* context)
{
    context->RegisterFactory<LightmapGenerator>();

    ATTRIBUTE("Pixels Per Unit", float, pixelsPerUnit_, 64, AM_DEFAULT);
    ATTRIBUTE("Blur Lightmap", bool, blurImage_, false, AM_DEFAULT);
    ATTRIBUTE("Bleed Radius", int, bleedRadius_, 5, AM_DEFAULT);
}

bool LightmapGenerator::GenerateLightmap()
{
    if (pixelsPerUnit_ <= 0)
    {
        LOGERROR("SceneLightmapGenerator::GenerateLightmap - invalid Pixels Per Unit value");
        return false;
    }

    sceneEditor_ = GetSceneEditor();
    if (sceneEditor_.Null())
    {
        LOGERROR("SceneLightmapGenerator::GenerateLightmap - unable to get scene editor");
        return false;
    }

    if (!InitPaths())
    {
        return false;
    }

    Vector<SharedPtr<StaticModelLightmapGenerator>> generators;
    GetModelGenerators(generators, pixelsPerUnit_);

    if (!generators.Size())
    {
        LOGERROR("SceneLightmapGenerator::GenerateLightmap - No lightmappable StaticMeshes found in scene");
        return false;
    }

    Scene* scene = GetScene();
    PODVector<Light*> lights;
    StaticModelLightmapGenerator::FindCompatibleLights(scene, lights);

    if (!lights.Size())
    {
        LOGERROR("SceneLightmapGenerator::GenerateLightmap - No lightmap compatible Lights found in scene");
        return false;
    }

    Vector<SharedPtr<Image>> images(generators.Size());
    Octree* octree = scene->GetComponent<Octree>();
    for (unsigned i = 0; i < generators.Size(); i++)
    {
        Image* genImage = generators[i]->GenerateLightmapImageInternal(octree, lights, bleedRadius_, blurImage_, false);
        if (NULL == genImage)
            return false;
        images[i] = genImage;
    }

    Vector<IntRect> rects;
    SharedPtr<ImageAtlasGenerator> atlasGenerator(new ImageAtlasGenerator(context_));
    SharedPtr<Image> atlas = atlasGenerator->GenerateAtlassedImage(images, rects);
    // Save logs error
    if (!atlas->SaveJPG(outputPathAbsolute_, 100))
        return false;

    // Reload as asset so models have correct file path
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    SharedPtr<Texture2D> atlasTexture(cache->GetResource<Texture2D>(resourcePath_));

    float atlasWidth = (float) atlas->GetWidth();
    float atlasHeight = (float) atlas->GetHeight();
    for (unsigned i = 0; i < images.Size(); i++)
    {
        IntRect& rect = rects[i];
        Vector2 scale((float)rect.Width() / atlasWidth, (float)rect.Height() / atlasHeight);
        Vector2 offset((float)rect.left_ / atlasWidth, (float)rect.top_ / atlasHeight);

        LMStaticModel* model = generators[i]->GetModel();
        model->SetLightmapTexure(atlasTexture);
        model->SetLightmapUVTransform(offset, 0, scale);
    }
    
    // Currently blocks, async in future
    SendEvent(E_LIGHTMAPGENERATEEND);

    return true;
}

bool LightmapGenerator::InitPaths()
{
    String scenePath = sceneEditor_->GetFullPath();

    String pathName;
    String fileName;
    String ext;
    SplitPath(scenePath, pathName, fileName, ext);

    outputPathAbsolute_ = pathName + "Lightmaps/";

    FileSystem* fileSystem = GetSubsystem<FileSystem>();
    if (!fileSystem->DirExists(outputPathAbsolute_))
    {
        if (!fileSystem->CreateDirs(pathName, "Lightmaps/"))
        {
            LOGERRORF("SceneLightmapGenerator::InitPaths - Unable to create path: %s", outputPathAbsolute_.CString());
            return false;
        }
    }

    outputPathAbsolute_ += fileName + ".png";

    Project* project = GetSubsystem<ToolSystem>()->GetProject();
    resourcePath_ = project->GetResourceRelativePath(outputPathAbsolute_);

    return true;
}

void LightmapGenerator::GetModelGenerators(Vector<SharedPtr<StaticModelLightmapGenerator>>& generators, float pixelsPerUnit)
{
    PODVector<Node*> modelNodes;
    GetScene()->GetChildrenWithComponent<LMStaticModel>(modelNodes, true);
    generators.Clear();
    PODVector<Node*>::ConstIterator nodeIter;
    for (nodeIter = modelNodes.Begin(); nodeIter != modelNodes.End(); nodeIter++)
    {
        if (!(*nodeIter)->IsEnabled())
            continue;
        
        LMStaticModel* model = (*nodeIter)->GetComponent<LMStaticModel>();
        if (NULL == model || !model->IsEnabled())
            continue;

        SharedPtr<StaticModelLightmapGenerator> generator(new StaticModelLightmapGenerator(model->GetContext()));
        if (!generator->PreprocessVertexData(model, pixelsPerUnit, false))
        {
            LOGWARNINGF("Failed to process lightmap vertex data for model with LIGHTMAP constant in technique. Node %s", (*nodeIter)->GetName().CString());
            continue;
        }

        generators.Push(generator);
    }
}