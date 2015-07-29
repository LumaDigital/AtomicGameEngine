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
#include "../Scene/Component.h"

namespace Atomic
{

class DynamicNavigationMesh;

/// Obstacle for dynamic navigation mesh.
class ATOMIC_API Obstacle : public Component
{
    OBJECT(Obstacle)
    friend class DynamicNavigationMesh;

public:
    /// Construct.
    Obstacle(Context*);
    /// Destruct.
    virtual ~Obstacle();

    /// Register Obstacle with engine context.
    static void RegisterObject(Context*);

    /// Update the owning mesh when enabled status has changed.
    virtual void OnSetEnabled();

    /// Get the height of this obstacle.
    float GetHeight() const { return height_; }
    /// Set the height of this obstacle.
    void SetHeight(float);
    /// Get the blocking radius of this obstacle.
    float GetRadius() const { return radius_; }
    /// Set the blocking radius of this obstacle.
    void SetRadius(float);

    /// Get the internal obstacle ID.
    unsigned GetObstacleID() const { return obstacleId_; }

    /// Render debug information.
    virtual void DrawDebugGeometry(DebugRenderer*, bool depthTest);
    /// Simplified rendering of debug information for script usage.
    void DrawDebugGeometry(bool depthTest);

protected:
    /// Handle scene being assigned, identify our DynamicNavigationMesh.
    virtual void OnSceneSet(Scene* scene);

private:
    /// Radius of this obstacle.
    float radius_;
    /// Height of this obstacle, extends 1/2 height below and 1/2 height above the owning node's position.
    float height_;

    /// Id received from tile cache.
    unsigned obstacleId_;
    /// Pointer to the navigation mesh we belong to.
    WeakPtr<DynamicNavigationMesh> ownerMesh_;
};

}
