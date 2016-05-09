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

#include "../Container/Ptr.h"
#include "../Scene/Scene.h"
#include "../Scene/Node.h"
#include "../Graphics/Material.h"
#include "../Graphics/Technique.h"
#include "../Graphics/Octree.h"
#include "../IO/Log.h"

#include "Model.h"
#include "StaticModelLightmapGenerator.h"
#include "SceneLightmapGenerator.h"
#include "ImageAtlasGenerator.h"

using namespace Atomic;

SceneLightmapGenerator::SceneLightmapGenerator(Context* context) :
    Object(context)
{
}

SceneLightmapGenerator::~SceneLightmapGenerator()
{
}

SharedPtr<Image> SceneLightmapGenerator::GenerateLightmapImage(Scene* scene, float pixelsPerUnit)
{
    Vector<SharedPtr<StaticModelLightmapGenerator>> generators;
    GetModelGenerators(scene, generators, pixelsPerUnit);

    if (!generators.Size())
    {
        LOGWARNING("No lightmappable StaticMeshes found in scene");
        return SharedPtr<Image>();
    }

    PODVector<Light*> lights;
    StaticModelLightmapGenerator::FindCompatibleLights(scene, lights);

    if (!lights.Size())
    {
        LOGWARNING("No lightmap compatible Lights found in scene");
        return SharedPtr<Image>();
    }

    Vector<SharedPtr<Image>> images(generators.Size());
    Octree* octree = scene->GetComponent<Octree>();
    for (unsigned i = 0; i < generators.Size(); i++)
    {
        images[i] = generators[i]->GenerateLightmapImageInternal(octree, lights, false);
    }

    Vector<IntRect> rects;
    SharedPtr<ImageAtlasGenerator> atlasGenerator(new ImageAtlasGenerator(context_));
    SharedPtr<Image> atlas = atlasGenerator->GenerateAtlassedImage(images, rects);
    float atlasWidth = (float) atlas->GetWidth();
    float atlasHeight = (float) atlas->GetHeight();
    for (unsigned i = 0; i < images.Size(); i++)
    {
        IntRect& rect = rects[i];
        Vector2 scale((float)rect.Width() / atlasWidth, (float)rect.Height() / atlasHeight);
        Vector2 offset((float)rect.left_ / atlasWidth, (float)rect.right_ / atlasHeight);
        generators[i]->GetModel()->SetLightmapUVScale(scale);
        generators[i]->GetModel()->SetLightmapUVOffset(offset);
    }

    return atlas;
}

void SceneLightmapGenerator::GetModelGenerators(Scene* scene, Vector<SharedPtr<StaticModelLightmapGenerator>>& generators, float pixelsPerUnit)
{
    PODVector<Node*> modelNodes;
    scene->GetChildrenWithComponent<StaticModel>(modelNodes, true);
    generators.Clear();
    PODVector<Node*>::ConstIterator nodeIter;
    for (nodeIter = modelNodes.Begin(); nodeIter != modelNodes.End(); nodeIter++)
    {
        if (!(*nodeIter)->IsEnabled())
            continue;
        
        StaticModel* model = (*nodeIter)->GetComponent<StaticModel>();
        if (NULL == model || !model->IsEnabled())
            continue;

        // TODO: Determine the right technique to use
        Technique* technique = model->GetMaterial()->GetTechnique(0);
        bool hasLightmapPass = false;
        for (unsigned i = 0; i < technique->GetNumPasses(); i++)
        {
            Pass* pass = technique->GetPass(i);
            if (NULL != pass &&
                (pass->GetPixelShaderDefines().Contains("LIGHTMAP") ||
                pass->GetVertexShaderDefines().Contains("LIGHTMAP")))
            {
                hasLightmapPass = true;
                break;
            }
        }

        if (!hasLightmapPass)
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