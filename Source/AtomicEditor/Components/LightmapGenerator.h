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

#pragma once
#include <Atomic/Container/Ptr.h>
#include <Atomic/Container/Vector.h>

#include "../Utils/StaticModelLightmapGenerator.h"

#include "EditorComponent.h"

namespace Atomic
{
    class Scene;
}

using namespace Atomic;

namespace AtomicEditor
{

class ATOMIC_API LightmapGenerator : public EditorComponent
{
    OBJECT(LightmapGenerator);

public:
    /// Construct.
    LightmapGenerator(Context* context);

    /// Destruct.
    ~LightmapGenerator();

    /// Register object factory.
    static void RegisterObject(Context* context);

    /// Generates a lightmap for the scene at the specified resolution. Returns the lightmap image if successful, null otherwise.
    bool GenerateLightmap();

protected:
    bool InitPaths();
    void GetModelGenerators(Vector<SharedPtr<StaticModelLightmapGenerator>>& generators, float pixelsPerUnit);

    float pixelsPerUnit_;
    WeakPtr<SceneEditor3D> sceneEditor_;
    String outputPathAbsolute_;
    String resourcePath_;

};
}

