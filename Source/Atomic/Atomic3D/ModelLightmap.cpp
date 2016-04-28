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

// TODO: Replace Image pixel accessors with direct buffer access

#include "../Container/Sort.h"
#include "../Scene/Node.h"
#include "../Scene/Scene.h"
#include "../Graphics/Geometry.h"
#include "../Graphics/GraphicsDefs.h"
#include "../Graphics/VertexBuffer.h"
#include "../Graphics/IndexBuffer.h"
#include "../Graphics/Octree.h"
#include "../Graphics/OctreeQuery.h"
#include "../Graphics/Light.h"

#include "ModelLightmap.h"
#include "Model.h"

using namespace Atomic;

Vector<Pair<int, int>> ModelLightmap::searchPattern_;

ModelLightmap::ModelLightmap(Context* context) :
    Object(context),
    coordSet_(0)
{
    if (searchPattern_.Empty())
        BuildSearchPattern();
}

ModelLightmap::~ModelLightmap()
{
}

void Atomic::ModelLightmap::LightTriangle(
    const Octree* octree, const PODVector<Light*> lights,
    const Vector3& p1, const Vector3& p2, const Vector3& p3, 
    const Vector3& n1, const Vector3& n2, const Vector3& n3, const Vector2& t1, 
    const Vector2& t2, const Vector2& t3)
{
    const float ambientValue = 0.0f;
    const Color ambientColor = Color(ambientValue, ambientValue, ambientValue);

    Vector2 tMin = t1; 
    tMin.ComponentwiseMin(t2);
    tMin.ComponentwiseMin(t3);
    Vector2 tMax = t1;
    tMax.ComponentwiseMax(t2);
    tMax.ComponentwiseMax(t3);

    int minX = GetPixelCoordinate(tMin.x_);
    int maxX = GetPixelCoordinate(tMax.x_);
    int minY = GetPixelCoordinate(tMin.y_);
    int maxY = GetPixelCoordinate(tMax.y_);

    Vector2 textureCoord;
    Vector3 barycentricCoords;
    Vector3 pos;
    Vector3 normal;
    Color color;
    for (int i = minX; i <= maxX; ++i)
    {
        for (int j = minY; j <= maxY; ++j)
        {
            textureCoord.x_ = GetTextureCoordinate(i);
            textureCoord.y_ = GetTextureCoordinate(j);
            barycentricCoords = GetBarycentricCoordinates(t1, t2, t3, textureCoord);

            if (image_->GetPixelInt(i, j, 1) == 1 || barycentricCoords.x_ < 0 || barycentricCoords.y_ < 0 || barycentricCoords.z_ < 0)
                continue;

            pos = barycentricCoords.x_ * p1 + barycentricCoords.y_ * p2 + barycentricCoords.z_ * p3;
            normal = barycentricCoords.x_ * n1 + barycentricCoords.y_ * n2 + barycentricCoords.z_ * n3;
            color = ambientColor;
            for (int k = 0; k < lights.Size(); ++k)
            {
                ApplyLight(octree, lights[k], pos, normal, color);
            }
            image_->SetPixelInt(i, j, 0, color.ToUInt());
            image_->SetPixelInt(i, j, 1, 1);
        }
    }
}

void Atomic::ModelLightmap::ApplyLight(const Octree* octree, const Light* light, const Vector3& pos, const Vector3& normal, Color& color)
{
    const float directionalCastDistance = 100.0f;

    if (light->GetLightType() == LightType::LIGHT_POINT)
    {
        const Vector3& lightPos = light->GetNode()->GetPosition();
        Vector3 lightDirection = lightPos - pos;
        float distanceSquared = lightDirection.LengthSquared();
        float range = light->GetRange();
        float rangeSquared = range * range;
        if (distanceSquared >= rangeSquared)
            return;
        
        lightDirection.Normalize();
        if (lightDirection.DotProduct(normal) < 0)
            return;

        if (distanceSquared > M_EPSILON)
        { 
            Ray ray(lightPos, lightDirection);
            PODVector<RayQueryResult> results;
            RayOctreeQuery query(results, ray, RAY_TRIANGLE, range, DRAWABLE_GEOMETRY);
            octree->RaycastSingle(query);

            if (!query.result_.Empty())
                return;

            // Opposite cast to ensure the light is visible. Necessary?
            /*Ray returnRay(pos, -lightDirection);
            RayOctreeQuery backQuery(results, ray, RAY_TRIANGLE, range, DRAWABLE_GEOMETRY);
            if (!backQuery.result_.Empty())
                return;*/

            float intensity = distanceSquared / rangeSquared;
            Color lightColor = light->GetColor().Lerp(Color::BLACK, intensity);
            color.r_ += lightColor.r_;
            color.g_ += lightColor.g_;
            color.b_ += lightColor.b_;
        }
        else
        {
            const Color& lightColor = light->GetColor();
            color.r_ += lightColor.r_;
            color.g_ += lightColor.g_;
            color.b_ += lightColor.b_;
        }
    }
    else
    {
        Vector3 lightDirection = light->GetNode()->GetWorldDirection();

        float intensity = -lightDirection.DotProduct(normal);
        if (intensity < 0)
            return;

        Vector3 origin = pos - directionalCastDistance * lightDirection;

        Ray ray(origin, lightDirection);
        PODVector<RayQueryResult> results;
        RayOctreeQuery query(results, ray, RAY_TRIANGLE, directionalCastDistance, DRAWABLE_GEOMETRY);
        octree->RaycastSingle(query);

        if (!query.result_.Empty())
            return;

        // Opposite cast to ensure the light is visible. Necessary?
        /*Ray returnRay(pos, -lightDirection);
        RayOctreeQuery backQuery(results, ray, RAY_TRIANGLE, directionalCastDistance, DRAWABLE_GEOMETRY);
        if (!backQuery.result_.Empty())
            return;*/

        const Color& lightColor = light->GetColor();
        color.r_ += intensity * lightColor.r_;
        color.g_ += intensity * lightColor.g_;
        color.b_ += intensity * lightColor.b_;
    }
}

void Atomic::ModelLightmap::CalculateLightMap(StaticModel* model, float pixelsPerUnit, int texSize, bool debugLightmaps, int limitToTriangle)
{
    pixelsPerUnit_ = pixelsPerUnit;
    texSize_ = texSize;
    debugLightmaps_ = debugLightmaps;

    // Get mesh data TODO: get all buffers, do we need to copy all this data?
    Geometry* geometry = model->GetModel()->GetGeometry(0, 0);
    VertexBuffer* vertexBuffer = geometry->GetVertexBuffer(0);

    assert(geometry);
    assert(vertexBuffer);

    unsigned elementMask = geometry->GetVertexElementMask(0);
    assert(elementMask & MASK_POSITION);
    assert(elementMask & MASK_NORMAL);
    assert(elementMask & MASK_TEXCOORD2);

    unsigned vertexSize = vertexBuffer->GetVertexSize();
    unsigned vertexCount = vertexBuffer->GetVertexCount();

    Vector<Vector3> meshPositions(vertexCount);
    Vector<Vector3> meshNormals(vertexCount);
    Vector<Vector2> meshUVs(vertexCount);
    
    Matrix3x4 worldTransform = model->GetNode()->GetWorldTransform();
    Quaternion normalRotation = worldTransform.Rotation();

    unsigned char* vertexData = vertexBuffer->GetShadowData();
    unsigned char* posData = vertexData + vertexBuffer->GetElementOffset(VertexElement::ELEMENT_POSITION);
    unsigned char* normalData = vertexData + vertexBuffer->GetElementOffset(VertexElement::ELEMENT_NORMAL);
    unsigned char* uvData = vertexData + vertexBuffer->GetElementOffset(VertexElement::ELEMENT_TEXCOORD2);
    for (unsigned i = 0; i < vertexCount; ++i)
    {
        meshPositions[i] = worldTransform*(*(const Vector3*)posData);
        meshNormals[i] = normalRotation*(*(const Vector3*)normalData);
        meshUVs[i] = *((const Vector2*)uvData);

        posData += vertexSize;
        normalData += vertexSize;
        uvData += vertexSize;
    }

    // Determine lightmap texture size
    IndexBuffer* indexBuffer = geometry->GetIndexBuffer();
    unsigned indexCount = indexBuffer->GetIndexCount();
    unsigned indices[3];
    bool useShortIndices = indexBuffer->GetIndexSize() == sizeof(unsigned short);
    unsigned short* shortIndexData = (unsigned short*) indexBuffer->GetShadowData();
    unsigned* indexData = (unsigned*) shortIndexData;
    if (pixelsPerUnit_ > 0.0f)
    {
        float surfaceArea = 0;
        for (unsigned i = 0; i < indexCount; i += 3)
        {
            if (useShortIndices)
            {
                indices[0] = shortIndexData[i];
                indices[1] = shortIndexData[i + 1];
                indices[2] = shortIndexData[i + 2];
            }
            else
            {
                indices[0] = indexData[i];
                indices[1] = indexData[i + 1];
                indices[2] = indexData[i + 2];
            }
            surfaceArea += GetTriangleArea(meshPositions[indices[0]], meshPositions[indices[1]], meshPositions[indices[2]]);
        }
        float texSize = sqrtf(surfaceArea)*pixelsPerUnit_;

        texSize_ = 1;
        while (texSize_ < texSize)
            texSize_ *= 2;
    }

    CreateTexture();

    // Get usable lightNodes
    PODVector<Node*> lightNodes;
    model->GetScene()->GetChildrenWithComponent<Light>(lightNodes, true);
    PODVector<Light*> lights;
    for (PODVector<Node*>::ConstIterator nodeIter = lightNodes.Begin(); nodeIter != lightNodes.End(); nodeIter++)
    {
        if (!(*nodeIter)->IsEnabled())
            continue;
        Light* light = (*nodeIter)->GetComponent<Light>();
        if (light &&
            light->IsEnabled() &&
            (light->GetLightType() == LightType::LIGHT_DIRECTIONAL ||
             light->GetLightType() == LightType::LIGHT_POINT))
            lights.Push(light);
    }
    assert(lights.Size());


    // Fill in the lightmap
    const Octree* octree = model->GetScene()->GetComponent<Octree>();
    unsigned triCount = indexCount / 3;
    for (unsigned i = 0; i < indexCount; i += 3)
    {

        if (limitToTriangle >= 0 && limitToTriangle < triCount && (i/3) != limitToTriangle)
            continue;

        if (useShortIndices)
        {
            indices[0] = shortIndexData[i];
            indices[1] = shortIndexData[i + 1];
            indices[2] = shortIndexData[i + 2];
        }
        else
        {
            indices[0] = indexData[i];
            indices[1] = indexData[i + 1];
            indices[2] = indexData[i + 2];
        }
        LightTriangle(
            octree, lights,
            meshPositions[indices[0]], meshPositions[indices[1]], meshPositions[indices[2]],
            meshNormals[indices[0]], meshNormals[indices[1]], meshNormals[indices[2]],
            meshUVs[indices[0]], meshUVs[indices[1]], meshUVs[indices[2]]);
    }

    //FillInvalidPixels();
    //image_->Blur(1.0f);

}

bool Atomic::ModelLightmap::SaveToFile(const String& fileName, int quality)
{
    return image_->SaveToFile(fileName, quality);
}

void Atomic::ModelLightmap::AssignMaterial()
{
    // TODO:
}

void Atomic::ModelLightmap::CreateTexture()
{
    if (!image_.Null())
        return;

    image_ = new Image(context_);
    image_->SetSize(texSize_, texSize_, 3);
}

void Atomic::ModelLightmap::FillInvalidPixels()
{
    Vector<Pair<int, int> >::Iterator itSearchPattern;
    for (int i = 0; i < texSize_; ++i)
    {
        for (int j = 0; j < texSize_; ++j)
        {
            // Invalid pixel found
            if (image_->GetPixelInt(i, j, 1) == 0)
            {
                for (itSearchPattern = searchPattern_.Begin(); itSearchPattern != searchPattern_.End(); ++itSearchPattern)
                {
                    int x = i + itSearchPattern->first_;
                    int y = j + itSearchPattern->second_;
                    if (x < 0 || x >= texSize_)
                        continue;
                    if (y < 0 || y >= texSize_)
                        continue;
                    // If search pixel is valid assign it to the invalid pixel and stop searching
                    if (image_->GetPixelInt(x, y, 1) == 1)
                    {
                        image_->SetPixelInt(i, j, image_->GetPixelInt(x, y));
                        break;
                    }
                }
            }
        }
    }
}

inline int Atomic::ModelLightmap::GetPixelCoordinate(float textureCoord)
{
    int pixel = (int)(textureCoord * texSize_);
    if (pixel < 0)
        pixel = 0;
    if (pixel >= texSize_)
        pixel = texSize_ - 1;
    return pixel;
}

inline float Atomic::ModelLightmap::GetTextureCoordinate(int pixelCoord)
{
    return (((float)pixelCoord) + 0.5f) / (float)texSize_;
}

Vector3 Atomic::ModelLightmap::GetBarycentricCoordinates(const Vector2& p1, const Vector2& p2, const Vector2& p3, const Vector2& p)
{
    Vector3 coords(0, 0);
    float denom = (-p1.x_ * p3.y_ - p2.x_ * p1.y_ + p2.x_ * p3.y_ + p1.y_ * p3.x_ + p2.y_ *p1.x_ - p2.y_ * p3.x_ );

    if (fabs(denom) > M_EPSILON)
    {
        coords.x_ = (p2.x_ * p3.y_ - p2.y_ * p3.x_ - p.x_ * p3.y_ + p3.x_ * p.y_ - p2.x_ * p.y_ + p2.y_ * p.x_) / denom;
        coords.y_ = -(-p1.x_ * p.y_ + p1.x_ * p3.y_ + p1.y_ * p.x_ - p.x_ * p3.y_ + p3.x_ * p.y_ - p1.y_ * p3.x_) / denom;
    }
    coords.z_ = 1 - coords.x_ - coords.y_;

    return coords;
}

float Atomic::ModelLightmap::GetTriangleArea(const Vector3& p1, const Vector3& p2, const Vector3& p3)
{
    return 0.5f * (p2 - p1).CrossProduct(p3 - p1).Length();
}

struct SortCoordsByDistance
{
    bool operator() (Pair<int, int> &left, Pair<int, int> &right)
    {
        return (left.first_*left.first_ + left.second_*left.second_) <
            (right.first_*right.first_ + right.second_*right.second_);
    }
};

void Atomic::ModelLightmap::BuildSearchPattern()
{
    const int size = 5;

    searchPattern_.Clear();
    for (int i = -size; i <= size; ++i)
    {
        for (int j = -size; j <= size; ++j)
        {
            if (i == 0 && j == 0)
                continue;
            searchPattern_.Push(Pair<int, int>(i, j));
        }
    }
    SortCoordsByDistance compare;
    Sort(searchPattern_.Begin(), searchPattern_.End(), compare);
}
