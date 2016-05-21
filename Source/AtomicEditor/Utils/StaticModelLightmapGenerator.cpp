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

#include <Atomic/Container/Sort.h>
#include <Atomic/Scene/Node.h>
#include <Atomic/Scene/Scene.h>
#include <Atomic/Graphics/Geometry.h>
#include <Atomic/Graphics/GraphicsDefs.h>
#include <Atomic/Graphics/VertexBuffer.h>
#include <Atomic/Graphics/IndexBuffer.h>
#include <Atomic/Graphics/Octree.h>
#include <Atomic/Graphics/OctreeQuery.h>
#include <Atomic/Graphics/Light.h>
#include <Atomic/IO/Log.h>
#include <Atomic/Atomic3D/Model.h>

#include "StaticModelLightmapGenerator.h"

using namespace Atomic;
using namespace AtomicEditor;

// Image::GetPixelInt applies full alpha if no alpha channel in the image
#define SET_PIXEL 0xff000001
#define UNSET_PIXEL 0xff000000

StaticModelLightmapGenerator::StaticModelLightmapGenerator(Context* context) :
    Object(context),
    model_(NULL),
    imageSize_(0),
    pixelsPerUnit_(0)
{
}

StaticModelLightmapGenerator::~StaticModelLightmapGenerator()
{
}

unsigned StaticModelLightmapGenerator::GetRequiredImageSize() const
{
    if (NULL == model_)
        LOGERROR("No processed vertex data available on lightmap generator, call PreprocessVertexData first");
    
    return imageSize_;
}

bool StaticModelLightmapGenerator::PreprocessVertexData(LMStaticModel* model, float pixelsPerUnit, bool powerOfTwo)
{
    assert(pixelsPerUnit > 0);
    assert(NULL != model);

    model_ = model;
    pixelsPerUnit_ = pixelsPerUnit;

    // Find the highest LOD with compatible vertex data
    VertexBuffer* vertexBuffer = NULL;
    IndexBuffer* indexBuffer = NULL;
    if (!GetCompatibleBuffers(model, &vertexBuffer, &indexBuffer))
        return false;
    assert(vertexBuffer);
    assert(indexBuffer);

    // Precalculate transformed data
    unsigned vertexCount = vertexBuffer->GetVertexCount();
    positions_.Resize(vertexCount);
    normals_.Resize(vertexCount);
    uvs_.Resize(vertexCount);

    unsigned vertexSize = vertexBuffer->GetVertexSize();
    const Matrix3x4& worldTransform = model->GetNode()->GetWorldTransform();
    Quaternion normalRotation = worldTransform.Rotation();

    unsigned char* vertexData = vertexBuffer->GetShadowData();
    unsigned char* posData = vertexData + vertexBuffer->GetElementOffset(VertexElement::ELEMENT_POSITION);
    unsigned char* normalData = vertexData + vertexBuffer->GetElementOffset(VertexElement::ELEMENT_NORMAL);
    unsigned char* uvData = vertexData + vertexBuffer->GetElementOffset(VertexElement::ELEMENT_TEXCOORD2);
    for (unsigned i = 0; i < vertexCount; ++i)
    {
        positions_[i] = worldTransform * (*(const Vector3*)posData);
        normals_[i] = normalRotation * (*(const Vector3*)normalData);
        uvs_[i] = *((const Vector2*)uvData);

        posData += vertexSize;
        normalData += vertexSize;
        uvData += vertexSize;
    }

    // Get unsigned indices
    unsigned indexCount = indexBuffer->GetIndexCount();
    indices_.Resize(indexCount);

    if (indexBuffer->GetIndexSize() == sizeof(unsigned short))
    {
        unsigned short* shortIndexData = (unsigned short*)indexBuffer->GetShadowData();
        for (unsigned i = 0; i < indexCount; ++i)
        {
            indices_[i] = *shortIndexData;
            shortIndexData++;
        }
    }
    else
    {
        unsigned* indexData = (unsigned*)indexBuffer->GetShadowData();
        for (unsigned i = 0; i < indexCount; ++i)
        {
            indices_[i] = *indexData;
            indexData++;
        }
    }

    // Determine the required image size
    imageSize_ = GetImageSize(pixelsPerUnit, positions_, indices_, powerOfTwo);

    return true;
}

SharedPtr<Image> StaticModelLightmapGenerator::GenerateLightmapImage(LMStaticModel* model, float pixelsPerUnit, int bleedRadius, bool blur)
{
    if ((model != model_ || pixelsPerUnit != pixelsPerUnit_ || !IsPowerOfTwo(imageSize_) ) && 
        !PreprocessVertexData(model, pixelsPerUnit, true))
        return SharedPtr<Image>();
        
    PODVector<Light*> lights;
    FindCompatibleLights(model->GetScene(), lights);

    return SharedPtr<Image>(GenerateLightmapImageInternal(model->GetScene()->GetComponent<Octree>(), lights, bleedRadius, blur, true));
}

Image* StaticModelLightmapGenerator::GenerateLightmapImageInternal(const Octree* octree, const PODVector<Light*>& lights, int bleedRadius, bool blur, bool powerOfTwo)
{
    Image* image = CreateImage();

    // Fill in the lightmap
    unsigned indexCount = indices_.Size();
    for (unsigned i = 0; i < indexCount; i += 3)
    {
        LightTriangle(
            octree, image, lights,
            positions_[indices_[i]], positions_[indices_[i+1]], positions_[indices_[i+2]],
            normals_[indices_[i]], normals_[indices_[i+1]], normals_[indices_[i+2]],
            uvs_[indices_[i]], uvs_[indices_[i+1]], uvs_[indices_[i+2]]);
    }

    Image* buffer = NULL;
    if (bleedRadius > 0)
    {
        buffer = CreateImage();
        FillInvalidPixels(image, buffer, bleedRadius);
        Swap(image, buffer);
    }

    if (blur)
    {
        if (!buffer)
            buffer = CreateImage();

        BlurImage(image, buffer);
        Swap(image, buffer);
    }

    if (buffer)
        delete(buffer);

    return image;
}

Image* AtomicEditor::StaticModelLightmapGenerator::CreateImage()
{
    Image* image = new Image(context_);
    image->SetSize(imageSize_, imageSize_, 2, 3);
    return image;
}

void StaticModelLightmapGenerator::LightTriangle(
    const Octree* octree, Image* image, const PODVector<Light*>& lights,
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

            if (image->GetPixelInt(i, j, 1) == SET_PIXEL || barycentricCoords.x_ < 0 || barycentricCoords.y_ < 0 || barycentricCoords.z_ < 0)
                continue;

            pos = barycentricCoords.x_ * p1 + barycentricCoords.y_ * p2 + barycentricCoords.z_ * p3;
            normal = barycentricCoords.x_ * n1 + barycentricCoords.y_ * n2 + barycentricCoords.z_ * n3;
            color = ambientColor;
            for (unsigned k = 0; k < lights.Size(); ++k)
            {
                Light* light = lights[k];
                if (light->GetLightType() == LightType::LIGHT_POINT)
                    ApplyPointLight(octree, light, pos, normal, color);
                else if (light->GetLightType() == LightType::LIGHT_DIRECTIONAL)
                    ApplyDirectionalLight(octree, light, pos, normal, color);
            }
            image->SetPixelInt(i, j, 0, color.ToUInt());
            image->SetPixelInt(i, j, 1, SET_PIXEL);
        }
    }
}

void StaticModelLightmapGenerator::ApplyPointLight(const Octree* octree, const Light* light, const Vector3& pos, const Vector3& normal, Color& color)
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


void StaticModelLightmapGenerator::ApplyDirectionalLight(const Octree* octree, const Light* light, const Vector3& pos, const Vector3& normal, Color& color)
{
    const float directionalCastDistance = 100.0f;

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

    const Color& lightColor = light->GetColor();
    color.r_ += intensity * lightColor.r_;
    color.g_ += intensity * lightColor.g_;
    color.b_ += intensity * lightColor.b_;
}

void StaticModelLightmapGenerator::FillInvalidPixels(Image* source, Image* target, int bleedRadius)
{
    Vector<Pair<int, int>> searchPattern;
    BuildSearchPattern(bleedRadius, searchPattern);

    int width = source->GetWidth();
    int height = source->GetWidth();
    Vector<Pair<int, int> >::Iterator iter;
    for (int i = 0; i < width; ++i)
    {
        for (int j = 0; j < height; ++j)
        {
            if (source->GetPixelInt(i, j, 1) == SET_PIXEL)
            {
                target->SetPixelInt(i, j, 0, source->GetPixelInt(i, j));
                target->SetPixelInt(i, j, 1, SET_PIXEL);
                continue;
            }

            // Invalid pixel found
            for (iter = searchPattern.Begin(); iter != searchPattern.End(); ++iter)
            {
                int x = i + iter->first_;
                int y = j + iter->second_;
                if (x < 0 || x >= width)
                    continue;
                if (y < 0 || y >= height)
                    continue;
                // If search pixel is valid assign it to the invalid pixel and stop searching
                if (source->GetPixelInt(x, y, 1) == SET_PIXEL)
                {
                    target->SetPixelInt(i, j, 0, source->GetPixelInt(x, y));
                    target->SetPixelInt(i, j, 1, SET_PIXEL);
                    break;
                }
            }
        }
    }
}

void StaticModelLightmapGenerator::BlurImage(Image* source, Image* target)
{
    int width = source->GetWidth();
    int height = source->GetHeight();

    Color color;
    float validPixels;
    int minK, maxK, minL, maxL;
    for (int i = 0; i < width; ++i)
    {
        for (int j = 0; j < height; ++j)
        {
            if (source->GetPixelInt(i, j, 1) == UNSET_PIXEL)
                continue;

            //blurredImage->SetPixelInt(i, j, image->GetPixelInt(i, j));
            target->SetPixelInt(i, j, 1, SET_PIXEL);
            color = Color::BLACK;
            validPixels = 0;
            
            minK = i - 1;
            if (minK < 0)
                minK = 0;
            maxK = i + 1;
            if (maxK >= width)
                maxK = width - 1;
            minL = j - 1;
            if (minL < 0)
                minL = 0;
            maxL = j + 1;
            if (maxL >= height)
                maxL = height - 1;

            for (int k = minK; k <= maxK; ++k)
            {
                for (int l = minL - 1; l < maxL; ++l)
                {
                    if (source->GetPixelInt(k, l, 1) == UNSET_PIXEL)
                        continue;

                    color += source->GetPixel(k, l, 0);
                    ++validPixels;
                }
            }
            color.r_ /= validPixels;
            color.g_ /= validPixels;
            color.b_ /= validPixels;
            target->SetPixel(i, j, color);
        }
    }
}

inline int StaticModelLightmapGenerator::GetPixelCoordinate(float textureCoord)
{
    int pixel = (int)(textureCoord * imageSize_);
    if (pixel < 0)
        pixel = 0;
    if (pixel >= (int)imageSize_)
        pixel = imageSize_ - 1;
    return pixel;
}

inline float StaticModelLightmapGenerator::GetTextureCoordinate(int pixelCoord)
{
    return (((float)pixelCoord) + 0.5f) / (float)imageSize_;
}

Vector3 StaticModelLightmapGenerator::GetBarycentricCoordinates(const Vector2& p1, const Vector2& p2, const Vector2& p3, const Vector2& p)
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

unsigned StaticModelLightmapGenerator::GetImageSize(float pixelsPerUnit, const PODVector<Vector3>& positions, const PODVector<unsigned>& indices, bool powerOfTwo)
{
    float surfaceArea = 0;
    unsigned indexCount = indices.Size();
    for (unsigned i = 0; i < indexCount; i += 3)
    {
        surfaceArea += GetTriangleArea(positions[indices[i]], positions[indices[i+1]], positions[indices[i+2]]);
    }
    float texSize = sqrtf(surfaceArea)*pixelsPerUnit;

    if (!powerOfTwo)
        return (int) ceilf(texSize);

    unsigned potTexSize = 1;
    while (potTexSize < texSize)
        potTexSize *= 2;

    return potTexSize;
}

float StaticModelLightmapGenerator::GetTriangleArea(const Vector3& p1, const Vector3& p2, const Vector3& p3)
{
    return 0.5f * (p2 - p1).CrossProduct(p3 - p1).Length();
}

bool StaticModelLightmapGenerator::GetCompatibleBuffers(LMStaticModel* model, VertexBuffer** vertexBuffer, IndexBuffer** indexBuffer)
{
    static const unsigned REQUIRED_ELEMENTS = MASK_POSITION | MASK_NORMAL | MASK_TEXCOORD2;

    const Vector<Vector<SharedPtr<Geometry> > >& modelGeometries = model->GetModel()->GetGeometries();
    Vector<Vector<SharedPtr<Geometry> > >::ConstIterator geometryIter;
    for (geometryIter = modelGeometries.Begin(); geometryIter != modelGeometries.End() && NULL == *vertexBuffer; geometryIter++)
    {
        const Vector<SharedPtr<Geometry>>& lods = *geometryIter;
        Vector<SharedPtr<Geometry>>::ConstIterator lodIter;
        for (lodIter = lods.Begin(); lodIter != lods.End() && NULL == *vertexBuffer; lodIter++)
        {
            Geometry* geometry = *lodIter;
            if (geometry)
            {
                int numBuffers = geometry->GetNumVertexBuffers();
                for (int i = 0; i < numBuffers; i++)
                {
                    if (geometry->GetVertexElementMask(i) & REQUIRED_ELEMENTS)
                    {
                        (*vertexBuffer) = geometry->GetVertexBuffer(i);
                        (*indexBuffer) = geometry->GetIndexBuffer();
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

void StaticModelLightmapGenerator::FindCompatibleLights(Scene* scene, PODVector<Light*>& lights)
{
    PODVector<Node*> lightNodes;
    scene->GetChildrenWithComponent<Light>(lightNodes, true);
    lights.Clear();
    PODVector<Node*>::ConstIterator nodeIter;
    for (nodeIter = lightNodes.Begin(); nodeIter != lightNodes.End(); nodeIter++)
    {
        if (!(*nodeIter)->IsEnabled())
            continue;
        Light* light = (*nodeIter)->GetComponent<Light>();
        if (NULL != light &&
            light->IsEnabled() &&
            (light->GetLightType() == LightType::LIGHT_DIRECTIONAL ||
                light->GetLightType() == LightType::LIGHT_POINT))
            lights.Push(light);
    }
}

struct CoordDistanceComparer
{
    bool operator() (Pair<int, int> &left, Pair<int, int> &right)
    {
        return (left.first_*left.first_ + left.second_*left.second_) <
            (right.first_*right.first_ + right.second_*right.second_);
    }
};

void StaticModelLightmapGenerator::BuildSearchPattern(int searchSize, Vector<Pair<int, int>>& searchPattern)
{
    searchPattern.Clear();
    for (int i = -searchSize; i <= searchSize; ++i)
    {
        for (int j = -searchSize; j <= searchSize; ++j)
        {
            if (i == 0 && j == 0)
                continue;
            searchPattern.Push(Pair<int, int>(i, j));
        }
    }
    CoordDistanceComparer comparer;
    Sort(searchPattern.Begin(), searchPattern.End(), comparer);
}