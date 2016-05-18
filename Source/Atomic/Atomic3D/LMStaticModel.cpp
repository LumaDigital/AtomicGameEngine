// Copyright (c) 2014-2015, THUNDERBEAST GAMES LLC All rights reserved
// Please see LICENSE.md in repository root for license information
// https://github.com/AtomicGameEngine/AtomicGameEngine

#include "Precompiled.h"
#include "../Core/Context.h"
#include "../Resource/ResourceCache.h"
#include "../Graphics/Technique.h"
#include "../Atomic3D/LMStaticModel.h"
#include "../IO/Log.h"
#include "../IO/FileSystem.h"

namespace Atomic
{

extern const char* GEOMETRY_CATEGORY;


LMStaticModel::LMStaticModel(Context* context) :
    StaticModel(context),
    lightmapUVOffset_(Vector2::ZERO),
    lightmapUVRepeat_(Vector2::ZERO),
    lightmapUVRotation_(0),
    lightmapUVTransform_(1.0f, 1.0f, 0.0f, 0.0f)
{

}

LMStaticModel::~LMStaticModel()
{

}

void LMStaticModel::RegisterObject(Context* context)
{
    context->RegisterFactory<LMStaticModel>(GEOMETRY_CATEGORY);

    COPY_BASE_ATTRIBUTES(StaticModel);
    MIXED_ACCESSOR_ATTRIBUTE("LightmapTexture", GetLightmapTextureAttr, SetLightmapTextureAttr, ResourceRef, ResourceRef(Texture2D::GetTypeStatic()), AM_DEFAULT);
    ACCESSOR_ATTRIBUTE("Lightmap UV Offset", GetLightmapUVOffsetAttr, SetLightmapUVOffsetAttr, Vector2, Vector2::ZERO, AM_DEFAULT);
    ACCESSOR_ATTRIBUTE("Lightmap UV Repeat", GetLightmapUVRepeatAttr, SetLightmapUVRepeatAttr, Vector2, Vector2::ZERO, AM_DEFAULT);
    ACCESSOR_ATTRIBUTE("Lightmap UV Rotation", GetLightmapUVRotationAttr, SetLightmapUVRotationAttr, float, 0.0f, AM_DEFAULT);
}

void LMStaticModel::SetMaterial(Material* material)
{
    StaticModel::SetMaterial(material);

    CreateLightmapMaterial();
}

bool LMStaticModel::SetMaterial(unsigned index, Material* material)
{
    bool success = StaticModel::SetMaterial(index, material);
    
    if (success && 0 == index)
        CreateLightmapMaterial();

    return success;
}

void LMStaticModel::SetLightmapTextureAttr(const ResourceRef& value)
{
    Texture2D* texture;
    if (value.name_.Length())
    {
        ResourceCache* cache = GetSubsystem<ResourceCache>();
        texture = cache->GetResource<Texture2D>(value.name_);
    }
    else
        texture = NULL;

    SetLightmapTexure(texture);

}

ResourceRef LMStaticModel::GetLightmapTextureAttr() const
{
    return GetResourceRef(lightmapTexture_, Texture2D::GetTypeStatic());
}

void LMStaticModel::SetLightmapTexure(Texture2D* texture)
{
    lightmapTexture_ = texture;

    // TODO: This is making a separate material for each model
    // we should be able to factor this into batching
    if (lightmapTexture_.NotNull())
    {
        if (lightmapMaterial_.Null())
            CreateLightmapMaterial();

        if (lightmapMaterial_.NotNull())
            lightmapMaterial_->SetTexture(TU_EMISSIVE, lightmapTexture_);
    }
}

void LMStaticModel::SetLightmapUVTransform(const Vector2& offset, float rotation, const Vector2& repeat)
{
    lightmapUVOffset_ = offset;
    lightmapUVRotation_ = rotation;
    lightmapUVRepeat_ = repeat;

    if (lightmapMaterial_.Null())
        CreateLightmapMaterial();

    if (lightmapMaterial_.NotNull())
        lightmapMaterial_->SetUVTransform2(lightmapUVOffset_, lightmapUVRotation_, lightmapUVRepeat_);
}

void LMStaticModel::SetLightmapUVOffsetAttr(const Vector2& offset)
{
    SetLightmapUVTransform(offset, lightmapUVRotation_, lightmapUVRepeat_);
}

void LMStaticModel::SetLightmapUVRepeatAttr(const Vector2& repeat)
{
    SetLightmapUVTransform(lightmapUVOffset_, lightmapUVRotation_, repeat);
}

void LMStaticModel::SetLightmapUVRotationAttr(float rotation)
{
    SetLightmapUVTransform(lightmapUVOffset_, rotation, lightmapUVRepeat_);
}

void LMStaticModel::SetLightmapUVTransform(const Vector4& transform)
{
    // WTF?
    lightmapUVTransform_ = transform;
}

const ResourceRefList& LMStaticModel::GetMaterialsAttr() const
{
    if (baseMaterial_)
    {
        materialsAttr_.names_.Resize(1);
        materialsAttr_.names_[0] = GetResourceName(baseMaterial_);

        return materialsAttr_;
    }
    else
        return StaticModel::GetMaterialsAttr();
}

void LMStaticModel::CreateLightmapMaterial()
{
    // TODO: Support for multiple materials and techniques?
    baseMaterial_ = GetMaterial(0);
    if (baseMaterial_)
    {
        lightmapMaterial_ = baseMaterial_;
        lightmapMaterial_ = baseMaterial_->Clone();
        Technique* baseTechnique = lightmapMaterial_->GetTechnique(0);
        const String& techniqueName = baseTechnique->GetName();
        if (techniqueName.Length() && !techniqueName.Contains("Lightmap", false))
        {
            ResourceCache* cache = GetSubsystem<ResourceCache>();
            String path, filename, extension;
            SplitPath(techniqueName, path, filename, extension);
            Technique* lmTechnique = cache->GetResource<Technique>(path + filename + "LightMap" + extension);
            if (lmTechnique)
                lightmapMaterial_->SetTechnique(0, lmTechnique);
            else
                LOGWARNINGF("No corresponding lightmap technique found for %s", filename);
        }
    }
}


void LMStaticModel::UpdateBatches(const FrameInfo& frame)
{
    StaticModel::UpdateBatches(frame);
    if (lightmapMaterial_.NotNull())
    {
        for (unsigned i = 0; i < batches_.Size(); ++i)
        {
            //batches_[i].lightmapTilingOffset_ = &lightmapTilingOffset_;
            batches_[i].geometryType_ = GEOM_STATIC_NOINSTANCING;

            batches_[i].material_ = lightmapMaterial_;
        }
    }
}
}