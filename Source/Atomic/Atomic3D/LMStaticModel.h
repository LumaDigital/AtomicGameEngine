// Copyright (c) 2014-2015, THUNDERBEAST GAMES LLC All rights reserved
// Please see LICENSE.md in repository root for license information
// https://github.com/AtomicGameEngine/AtomicGameEngine

#pragma once

#include "../Atomic3D/StaticModel.h"
#include "../Graphics/Texture2D.h"
#include "../Graphics/Material.h"

namespace Atomic
{

class LMStaticModel: public StaticModel
{
    OBJECT(LMStaticModel);

public:
    /// Construct.
    LMStaticModel(Context* context);
    /// Destruct.
    virtual ~LMStaticModel();

    /// Register object factory. StaticModel must be registered first.
    static void RegisterObject(Context* context);
    /// Set material on all geometries.
    virtual void SetMaterial(Material* material);
    /// Set material on one geometry. Return true if successful.
    virtual bool SetMaterial(unsigned index, Material* material);

    virtual void UpdateBatches(const FrameInfo& frame);

    void SetLightmapTexure(Texture2D* texture);
    void SetLightmapTextureAttr(const ResourceRef& value);
    ResourceRef GetLightmapTextureAttr() const;

    void SetLightmapUVTransform(const Vector2& offset, float rotation, const Vector2& repeat);
    void SetLightmapUVOffsetAttr(const Vector2& offset);
    const Vector2& SetLightmapUVOffsetAttr() const { return lightmapUVOffset_; }
    void SetLightmapUVRepeatAttr(const Vector2& repeat);
    const Vector2& SetLightmapUVRepeatAttr() const { return lightmapUVRepeat_; }
    void SetLightmapUVRepeatAttr(float rotation);
    float SetLightmapUVRotationAttr() const { return lightmapUVRotation_; }

    void SetLightmapUVTransform(const Vector4& transform);

    /// Return materials attribute.
    virtual const ResourceRefList& GetMaterialsAttr() const;

private:
    void CreateLightmapMaterial();

    SharedPtr<Texture2D> lightmapTexture_;
    SharedPtr<Material> lightmapMaterial_;
    SharedPtr<Material> baseMaterial_;

    Vector2 lightmapUVOffset_;
    Vector2 lightmapUVRepeat_;
    float lightmapUVRotation_;

    // TODO: Really not sure how this would be used?
    Vector4 lightmapUVTransform_;
};

}
