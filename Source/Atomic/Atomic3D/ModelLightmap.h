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
#include "../Container/Pair.h"
#include "../Container/Vector.h"
#include "../Math/Vector3.h"
#include "../Graphics/Texture2D.h"
#include "../Graphics/Material.h"
#include "../Resource/Image.h"

#include "StaticModel.h"

namespace Atomic
{
class ATOMIC_API ModelLightmap : public Object
{
    OBJECT(ModelLightmap);
    BASEOBJECT(ModelLightmap);
public:
    ModelLightmap(Context* context);
    ~ModelLightmap();

    void CalculateLightMap(StaticModel* model, float pixelsPerUnit/* = 0*/, int texSize/* = 512*/, bool debugLightmaps/* = false*/, int limitToTriangle/* = -1 */);
    bool SaveToFile(const String& fileName, int quality);

protected:
    void LightTriangle(
        const Octree* octree, const PODVector<Light*> lights,
        const Vector3& p1, const Vector3& p2, const Vector3& p3,
        const Vector3& n1, const Vector3& n2, const Vector3& n3,
        const Vector2& t1, const Vector2& t2, const Vector2& t3);
    void ApplyLight(const Octree* octree, const Light* light, const Vector3& pos, const Vector3& normal, Color& color);
    void AssignMaterial();
    void CreateTexture();
    void FillInvalidPixels();

    // Convert between texture coords given as real and pixel coords given as integers
    inline int GetPixelCoordinate(float textureCoord);
    inline float GetTextureCoordinate(int pixelCoord);

    // Calculate coords of P in terms of p1, p2 and p3
    // p = x*p1 + y*p2 + z*p3
    // If any of p.x, p.y or p.z are negative point is outside of triangle
    Vector3 GetBarycentricCoordinates(const Vector2& p1, const Vector2& p2, const Vector2& p3, const Vector2& p);
    float GetTriangleArea(const Vector3& p1, const Vector3& p2, const Vector3& p3);

    static void BuildSearchPattern();

    SharedPtr<Material> material_;
    SharedPtr<Image> image_;
    int texSize_;
    int coordSet_;
    float pixelsPerUnit_;
    bool debugLightmaps_;
    
    static Vector<Pair<int, int>> searchPattern_;
};
}

