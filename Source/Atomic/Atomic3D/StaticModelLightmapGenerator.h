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
#include "../Container/Ptr.h"
#include "../Container/Vector.h"
#include "../Math/Vector3.h"
#include "../Resource/Image.h"

#include "StaticModel.h"

namespace Atomic
{
    class Scene;
    class StaticModel;;
    class VertexBuffer;
    class IndexBuffer;
    class Octree;
    class Light;

class ATOMIC_API StaticModelLightmapGenerator : public Object
{
    OBJECT(StaticModelLightmapGenerator);
    BASEOBJECT(StaticModelLightmapGenerator);

    friend class SceneLightmapGenerator;

public:
    StaticModelLightmapGenerator(Context* context);
    ~StaticModelLightmapGenerator();

    /// Returns the size of the image that will be required to generate a lightmap for the model. Requires that PreprocessVertexData has been called.
    unsigned GetRequiredImageSize() const;
    /// Returns the model last used to precalculate vertex data and required image size.
    StaticModel* GetModel() const { return model_; }

    /// Calculates transformed vertex data and determines required image size. Returns true if compatible vertex data exists on the model, false otherwise.
    bool PreprocessVertexData(StaticModel* model, float pixelsPerUnit, bool powerOfTwo);
    /// Generates a lightmap for the model at the specified resolution. Returns the lightmap image if successful, null otherwise.
    SharedPtr<Image> GenerateLightmapImage(StaticModel* model, float pixelsPerUnit);

protected:
    Image* GenerateLightmapImageInternal(const Octree* octree, const PODVector<Light*>& lights, bool powerOfTwo);

    void LightTriangle(
        const Octree* octree, Image* image, const PODVector<Light*>& lights,
        const Vector3& p1, const Vector3& p2, const Vector3& p3,
        const Vector3& n1, const Vector3& n2, const Vector3& n3,
        const Vector2& t1, const Vector2& t2, const Vector2& t3);
    void ApplyLight(const Octree* octree, const Light* light, const Vector3& pos, const Vector3& normal, Color& color);
    static void FillInvalidPixels();
    static void BlurImage();

    // Convert between texture coords given as real and pixel coords given as integers
    inline int GetPixelCoordinate(float textureCoord);
    inline float GetTextureCoordinate(int pixelCoord);

    // Calculate coords of P in terms of p1, p2 and p3
    // p = x*p1 + y*p2 + z*p3
    // If any of p.x, p.y or p.z are negative point is outside of triangle
    static Vector3 GetBarycentricCoordinates(const Vector2& p1, const Vector2& p2, const Vector2& p3, const Vector2& p);
    static unsigned GetImageSize(float pixelsPerUnit, const PODVector<Vector3>& positions, const PODVector<unsigned>& indices, bool powerOfTwo);
    static float GetTriangleArea(const Vector3& p1, const Vector3& p2, const Vector3& p3);
    static bool GetCompatibleBuffers(StaticModel* model, VertexBuffer** vertexBuffer, IndexBuffer** indexBuffer);
    static void FindCompatibleLights(Scene* scene, PODVector<Light*>& lights);
    static void BuildSearchPattern();

    SharedPtr<StaticModel> model_;
    PODVector<Vector3> positions_;
    PODVector<Vector3> normals_;
    PODVector<Vector2> uvs_;
    PODVector<unsigned> indices_;
    unsigned imageSize_;
    float pixelsPerUnit_;

    static Vector<Pair<int, int>> searchPattern_;
};
}

