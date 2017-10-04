//
// Copyright (c) 2008-2017 the Urho3D project.
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

#include "../Precompiled.h"

#include "../Graphics/AnimatedModel.h"
#include "../Graphics/Animation.h"
#include "../Graphics/AnimationState.h"
#include "../Graphics/DrawableEvents.h"
#include "../IO/Log.h"

#include "../DebugNew.h"

namespace Atomic
{

AnimationStateTrack::AnimationStateTrack() :
    track_(0),
    bone_(0),
    weight_(1.0f),
    keyFrame_(0)
{
}

AnimationStateTrack::~AnimationStateTrack()
{
}

AnimationState::AnimationState(AnimatedModel* model, Animation* animation) :
    model_(model),
    animation_(animation),
    startBone_(0),
    looped_(false),
    weight_(0.0f),
    time_(0.0f),
    layer_(0),
    blendingMode_(ABM_LERP)
{
    // Set default start bone (use all tracks.)
    SetStartBone(0);

    // LUMA BEGIN

    ResetRootMotionOffset();
    Bone* rootBone = model_->GetSkeleton().GetRootBone();
    WeakPtr<Node> rootBoneNode = rootBone->node_;
    transformRoot_ = WeakPtr<Node>(rootBoneNode->GetChild("IHP_Master_01:EXP_J_Origin", true));

    // LUMA END
}

AnimationState::AnimationState(Node* node, Animation* animation) :
    node_(node),
    animation_(animation),
    startBone_(0),
    looped_(false),
    weight_(1.0f),
    time_(0.0f),
    layer_(0),
    blendingMode_(ABM_LERP)
{
    if (animation_)
    {
        // Setup animation track to scene node mapping
        if (node_)
        {
            const HashMap<StringHash, AnimationTrack>& tracks = animation_->GetTracks();
            stateTracks_.Clear();

            for (HashMap<StringHash, AnimationTrack>::ConstIterator i = tracks.Begin(); i != tracks.End(); ++i)
            {
                const StringHash& nameHash = i->second_.nameHash_;
                AnimationStateTrack stateTrack;
                stateTrack.track_ = &i->second_;

                if (node_->GetNameHash() == nameHash || tracks.Size() == 1)
                    stateTrack.node_ = node_;
                else
                {
                    Node* targetNode = node_->GetChild(nameHash, true);
                    if (targetNode)
                        stateTrack.node_ = targetNode;
                    else
                        ATOMIC_LOGWARNING("Node " + i->second_.name_ + " not found for node animation " + animation_->GetName());
                }

                if (stateTrack.node_)
                    stateTracks_.Push(stateTrack);
            }
        }
    }
}


AnimationState::~AnimationState()
{
}

void AnimationState::SetStartBone(Bone* startBone)
{
    if (!model_ || !animation_)
        return;

    Skeleton& skeleton = model_->GetSkeleton();
    if (!startBone)
    {
        Bone* rootBone = skeleton.GetRootBone();
        if (!rootBone)
            return;
        startBone = rootBone;
    }

    // Do not reassign if the start bone did not actually change, and we already have valid bone nodes
    if (startBone == startBone_ && !stateTracks_.Empty())
        return;

    startBone_ = startBone;

    const HashMap<StringHash, AnimationTrack>& tracks = animation_->GetTracks();
    stateTracks_.Clear();

    if (!startBone->node_)
        return;

    for (HashMap<StringHash, AnimationTrack>::ConstIterator i = tracks.Begin(); i != tracks.End(); ++i)
    {
        AnimationStateTrack stateTrack;
        stateTrack.track_ = &i->second_;

        // Include those tracks that are either the start bone itself, or its children
        Bone* trackBone = 0;
        const StringHash& nameHash = i->second_.nameHash_;

        if (nameHash == startBone->nameHash_)
            trackBone = startBone;
        else
        {
            Node* trackBoneNode = startBone->node_->GetChild(nameHash, true);
            if (trackBoneNode)
                trackBone = skeleton.GetBone(nameHash);
        }

        if (trackBone && trackBone->node_)
        {
            stateTrack.bone_ = trackBone;
            stateTrack.node_ = trackBone->node_;
            stateTracks_.Push(stateTrack);
        }
    }

    model_->MarkAnimationDirty();
}

void AnimationState::SetLooped(bool looped)
{
    looped_ = looped;
}

void AnimationState::SetWeight(float weight)
{
    // Weight can only be set in model mode. In node animation it is hardcoded to full
    if (model_)
    {
        weight = Clamp(weight, 0.0f, 1.0f);
        if (weight != weight_)
        {
            weight_ = weight;
            model_->MarkAnimationDirty();
        }
    }
}

void AnimationState::SetBlendMode(AnimationBlendMode mode)
{
    if (model_)
    {
        if (blendingMode_ != mode)
        {
            blendingMode_ = mode;
            model_->MarkAnimationDirty();
        }
    }
}

void AnimationState::SetTime(float time)
{
    if (!animation_)
        return;

    time = Clamp(time, 0.0f, animation_->GetLength());
    if (time != time_)
    {
        time_ = time;
        if (model_)
            model_->MarkAnimationDirty();

        // LUMA BEGIN

        if (time == 0.0f)
        {
            ResetRootMotionOffset();
        }

        // LUMA END
    }
}

void AnimationState::SetBoneWeight(unsigned index, float weight, bool recursive)
{
    if (index >= stateTracks_.Size())
        return;

    weight = Clamp(weight, 0.0f, 1.0f);

    if (weight != stateTracks_[index].weight_)
    {
        stateTracks_[index].weight_ = weight;
        if (model_)
            model_->MarkAnimationDirty();
    }

    if (recursive)
    {
        Node* boneNode = stateTracks_[index].node_;
        if (boneNode)
        {
            const Vector<SharedPtr<Node> >& children = boneNode->GetChildren();
            for (unsigned i = 0; i < children.Size(); ++i)
            {
                unsigned childTrackIndex = GetTrackIndex(children[i]);
                if (childTrackIndex != M_MAX_UNSIGNED)
                    SetBoneWeight(childTrackIndex, weight, true);
            }
        }
    }
}

void AnimationState::SetBoneWeight(const String& name, float weight, bool recursive)
{
    SetBoneWeight(GetTrackIndex(name), weight, recursive);
}

void AnimationState::SetBoneWeight(StringHash nameHash, float weight, bool recursive)
{
    SetBoneWeight(GetTrackIndex(nameHash), weight, recursive);
}

void AnimationState::AddWeight(float delta)
{
    if (delta == 0.0f)
        return;

    SetWeight(GetWeight() + delta);
}

void AnimationState::AddTime(float delta)
{
    if (!animation_ || (!model_ && !node_))
        return;

    float length = animation_->GetLength();
    if (delta == 0.0f || length == 0.0f)
        return;

    bool sendFinishEvent = false;

    float oldTime = GetTime();
    float time = oldTime + delta;
    if (looped_)
    {
        while (time >= length)
        {
            time -= length;

            // LUMA BEGIN
            ResetRootMotionOffset();
            // LUMA END

            sendFinishEvent = true;
        }
        while (time < 0.0f)
        {
            time += length;
            sendFinishEvent = true;
        }
    }

    SetTime(time);

    if (!looped_)
    {
        if (delta > 0.0f && oldTime < length && GetTime() == length)
        {
            sendFinishEvent = true;
        }
        else if (delta < 0.0f && oldTime > 0.0f && GetTime() == 0.0f)
            sendFinishEvent = true;
    }

    // Process finish event
    if (sendFinishEvent)
    {
        using namespace AnimationFinished;

        WeakPtr<AnimationState> self(this);
        WeakPtr<Node> senderNode(model_ ? model_->GetNode() : node_);

        VariantMap& eventData = senderNode->GetEventDataMap();
        eventData[P_NODE] = senderNode;
        eventData[P_ANIMATION] = animation_;
        eventData[P_NAME] = animation_->GetAnimationName();
        eventData[P_LOOPED] = looped_;

        // Note: this may cause arbitrary deletion of animation states, including the one we are currently processing
        senderNode->SendEvent(E_ANIMATIONFINISHED, eventData);
        if (senderNode.Expired() || self.Expired())
            return;
    }

    // Process animation triggers
    if (animation_->GetNumTriggers())
    {
        bool wrap = false;

        if (delta > 0.0f)
        {
            if (oldTime > time)
            {
                oldTime -= length;
                wrap = true;
            }
        }
        if (delta < 0.0f)
        {
            if (time > oldTime)
            {
                time -= length;
                wrap = true;
            }
        }
        if (oldTime > time)
            Swap(oldTime, time);

        const Vector<AnimationTriggerPoint>& triggers = animation_->GetTriggers();
        for (Vector<AnimationTriggerPoint>::ConstIterator i = triggers.Begin(); i != triggers.End(); ++i)
        {
            float frameTime = i->time_;
            if (looped_ && wrap)
                frameTime = fmodf(frameTime, length);

            if (oldTime <= frameTime && time > frameTime)
            {
                using namespace AnimationTrigger;

                WeakPtr<AnimationState> self(this);
                WeakPtr<Node> senderNode(model_ ? model_->GetNode() : node_);

                VariantMap& eventData = senderNode->GetEventDataMap();
                eventData[P_NODE] = senderNode;
                eventData[P_ANIMATION] = animation_;
                eventData[P_NAME] = animation_->GetAnimationName();
                eventData[P_TIME] = i->time_;
                eventData[P_DATA] = i->data_;

                // Note: this may cause arbitrary deletion of animation states, including the one we are currently processing
                senderNode->SendEvent(E_ANIMATIONTRIGGER, eventData);
                if (senderNode.Expired() || self.Expired())
                    return;
            }
        }
    }
}

void AnimationState::SetLayer(unsigned char layer)
{
    if (layer != layer_)
    {
        layer_ = layer;
        if (model_)
            model_->MarkAnimationOrderDirty();
    }
}

AnimatedModel* AnimationState::GetModel() const
{
    return model_;
}

Node* AnimationState::GetNode() const
{
    return node_;
}

Bone* AnimationState::GetStartBone() const
{
    return model_ ? startBone_ : 0;
}

float AnimationState::GetBoneWeight(unsigned index) const
{
    return index < stateTracks_.Size() ? stateTracks_[index].weight_ : 0.0f;
}

float AnimationState::GetBoneWeight(const String& name) const
{
    return GetBoneWeight(GetTrackIndex(name));
}

float AnimationState::GetBoneWeight(StringHash nameHash) const
{
    return GetBoneWeight(GetTrackIndex(nameHash));
}

unsigned AnimationState::GetTrackIndex(const String& name) const
{
    for (unsigned i = 0; i < stateTracks_.Size(); ++i)
    {
        Node* node = stateTracks_[i].node_;
        if (node && node->GetName() == name)
            return i;
    }

    return M_MAX_UNSIGNED;
}

unsigned AnimationState::GetTrackIndex(Node* node) const
{
    for (unsigned i = 0; i < stateTracks_.Size(); ++i)
    {
        if (stateTracks_[i].node_ == node)
            return i;
    }

    return M_MAX_UNSIGNED;
}

unsigned AnimationState::GetTrackIndex(StringHash nameHash) const
{
    for (unsigned i = 0; i < stateTracks_.Size(); ++i)
    {
        Node* node = stateTracks_[i].node_;
        if (node && node->GetNameHash() == nameHash)
            return i;
    }

    return M_MAX_UNSIGNED;
}

float AnimationState::GetLength() const
{
    return animation_ ? animation_->GetLength() : 0.0f;
}

void AnimationState::Apply()
{
    if (!animation_ || !IsEnabled())
        return;

    if (model_)
        ApplyToModel();
    else
        ApplyToNodes();
}

void AnimationState::ApplyToModel()
{
    for (Vector<AnimationStateTrack>::Iterator i = stateTracks_.Begin(); i != stateTracks_.End(); ++i)
    {
        AnimationStateTrack& stateTrack = *i;
        float finalWeight = weight_ * stateTrack.weight_;

        // Do not apply if zero effective weight or the bone has animation disabled
        if (Equals(finalWeight, 0.0f) || !stateTrack.bone_->animated_)
            continue;

        // LUMA BEGIN
        if (animation_->GetApplyRootTransform() && transformRoot_ != NULL && stateTrack.node_ == transformRoot_)
        {
            ApplyRootMotion(stateTrack, finalWeight);
        }
        else
        {
            ApplyTrack(stateTrack, finalWeight, true);
        }
        // LUMA END
    }
}

void AnimationState::ApplyToNodes()
{
    // When applying to a node hierarchy, can only use full weight (nothing to blend to)
    for (Vector<AnimationStateTrack>::Iterator i = stateTracks_.Begin(); i != stateTracks_.End(); ++i)
        ApplyTrack(*i, 1.0f, false);
}

void AnimationState::ApplyTrack(AnimationStateTrack& stateTrack, float weight, bool silent)
{
    const AnimationTrack* track = stateTrack.track_;
    Node* node = stateTrack.node_;

    if (track->keyFrames_.Empty() || !node)
        return;

    unsigned& frame = stateTrack.keyFrame_;
    track->GetKeyFrameIndex(time_, frame);

    // Check if next frame to interpolate to is valid, or if wrapping is needed (looping animation only)
    unsigned nextFrame = frame + 1;
    bool interpolate = true;
    if (nextFrame >= track->keyFrames_.Size())
    {
        if (!looped_)
        {
            nextFrame = frame;
            interpolate = false;
        }
        else
            nextFrame = 0;
    }

    const AnimationKeyFrame* keyFrame = &track->keyFrames_[frame];
    unsigned char channelMask = track->channelMask_;

    Vector3 newPosition;
    Quaternion newRotation;
    Vector3 newScale;

    if (interpolate)
    {
        const AnimationKeyFrame* nextKeyFrame = &track->keyFrames_[nextFrame];
        float timeInterval = nextKeyFrame->time_ - keyFrame->time_;
        if (timeInterval < 0.0f)
            timeInterval += animation_->GetLength();
        float t = timeInterval > 0.0f ? (time_ - keyFrame->time_) / timeInterval : 1.0f;

        if (channelMask & CHANNEL_POSITION)
            newPosition = keyFrame->position_.Lerp(nextKeyFrame->position_, t);
        if (channelMask & CHANNEL_ROTATION)
            newRotation = keyFrame->rotation_.Slerp(nextKeyFrame->rotation_, t);
        if (channelMask & CHANNEL_SCALE)
            newScale = keyFrame->scale_.Lerp(nextKeyFrame->scale_, t);
    }
    else
    {
        if (channelMask & CHANNEL_POSITION)
            newPosition = keyFrame->position_;
        if (channelMask & CHANNEL_ROTATION)
            newRotation = keyFrame->rotation_;
        if (channelMask & CHANNEL_SCALE)
            newScale = keyFrame->scale_;
    }

    if (blendingMode_ == ABM_ADDITIVE) // not ABM_LERP
    {
        if (channelMask & CHANNEL_POSITION)
        {
            Vector3 delta = newPosition - stateTrack.bone_->initialPosition_;
            newPosition = node->GetPosition() + delta * weight;
        }
        if (channelMask & CHANNEL_ROTATION)
        {
            Quaternion delta = newRotation * stateTrack.bone_->initialRotation_.Inverse();
            newRotation = (delta * node->GetRotation()).Normalized();
            if (!Equals(weight, 1.0f))
                newRotation = node->GetRotation().Slerp(newRotation, weight);
        }
        if (channelMask & CHANNEL_SCALE)
        {
            Vector3 delta = newScale - stateTrack.bone_->initialScale_;
            newScale = node->GetScale() + delta * weight;
        }
    }
    else
    {
        if (!Equals(weight, 1.0f)) // not full weight
        {
            if (channelMask & CHANNEL_POSITION)
                newPosition = node->GetPosition().Lerp(newPosition, weight);
            if (channelMask & CHANNEL_ROTATION)
                newRotation = node->GetRotation().Slerp(newRotation, weight);
            if (channelMask & CHANNEL_SCALE)
                newScale = node->GetScale().Lerp(newScale, weight);
        }
    }

    if (silent)
    {
        if (channelMask & CHANNEL_POSITION)
            node->SetPositionSilent(newPosition);
        if (channelMask & CHANNEL_ROTATION)
            node->SetRotationSilent(newRotation);
        if (channelMask & CHANNEL_SCALE)
            node->SetScaleSilent(newScale);
    }
    else
    {
        if (channelMask & CHANNEL_POSITION)
            node->SetPosition(newPosition);
        if (channelMask & CHANNEL_ROTATION)
            node->SetRotation(newRotation);
        if (channelMask & CHANNEL_SCALE)
            node->SetScale(newScale);
    }
}

// LUMA BEGIN

void AnimationState::ApplyRootMotion(AnimationStateTrack& stateTrack, float weight)
{
    if (transformRoot_ == NULL)
    {
        return;
    }

    const AnimationTrack* track = stateTrack.track_;
    Node* node = stateTrack.node_;

    if (track->keyFrames_.Empty() || !node)
        return;

    unsigned& frame = stateTrack.keyFrame_;
    track->GetKeyFrameIndex(time_, frame);

    // Check if next frame to interpolate to is valid, or if wrapping is needed (looping animation only)
    unsigned nextFrame = frame + 1;
    bool interpolate = true;
    if (nextFrame >= track->keyFrames_.Size())
    {
        if (!looped_)
        {
            nextFrame = frame;
            interpolate = false;
        }
        else
            nextFrame = 0;
    }

    const AnimationKeyFrame* keyFrame = &track->keyFrames_[frame];
    unsigned char channelMask = track->channelMask_;

    Vector3 newPosition;
    Quaternion newRotation;
    Vector3 newScale;

    if (interpolate)
    {
        const AnimationKeyFrame* nextKeyFrame = &track->keyFrames_[nextFrame];
        float timeInterval = nextKeyFrame->time_ - keyFrame->time_;
        if (timeInterval < 0.0f)
            timeInterval += animation_->GetLength();
        float t = timeInterval > 0.0f ? (time_ - keyFrame->time_) / timeInterval : 1.0f;

        if (channelMask & CHANNEL_POSITION)
            newPosition = keyFrame->position_.Lerp(nextKeyFrame->position_, t);
        if (channelMask & CHANNEL_ROTATION)
            newRotation = keyFrame->rotation_.Slerp(nextKeyFrame->rotation_, t);
    }
    else
    {
        if (channelMask & CHANNEL_POSITION)
            newPosition = keyFrame->position_;
        if (channelMask & CHANNEL_ROTATION)
            newRotation = keyFrame->rotation_;
    }

    Vector3 offsetRotation = rootNodeRotationOffset_.EulerAngles();

    // TODO - remve these commented out debug messages when we're happy that it all works as desired.
    /*ATOMIC_LOGDEBUG("**************");
    ATOMIC_LOGDEBUGF("Root Motion [%s] time - %f / %f - weight %f", animation_->GetAnimationName().CString(), time_, animation_->GetLength(), weight);
    ATOMIC_LOGDEBUGF("Start - OffsetPos - (%f, %f, %f)", rootNodePositionOffset_.x_, rootNodePositionOffset_.y_, rootNodePositionOffset_.z_);
    ATOMIC_LOGDEBUGF("Start - OffsetRot - (%f, %f, %f)", offsetRotation.x_, offsetRotation.y_, offsetRotation.z_);*/

    Vector3 deltaPos = newPosition - rootNodePositionOffset_;

    //ATOMIC_LOGDEBUGF("Delta Pos - (%f, %f, %f)", deltaPos.x_, deltaPos.y_, deltaPos.z_);

    // to get this right, first cancel out the rotation caused by this animation track, then apply the translation LOCALLY so
    // that any non-animation related animation still applies to the translation, then finally apply the latest animation.
    model_->node_->Rotate(rootNodeRotationOffset_.Inverse(), Atomic::TS_WORLD);
    model_->node_->Translate(deltaPos, Atomic::TS_LOCAL);
    model_->node_->Rotate(newRotation, Atomic::TS_WORLD);

    rootNodePositionOffset_ = newPosition;
    rootNodeRotationOffset_ = newRotation;

    offsetRotation = rootNodeRotationOffset_.EulerAngles();

    Vector3 modelPosition = model_->node_->GetWorldPosition();
    Vector3 modelRotation = model_->node_->GetWorldRotation().EulerAngles();

    Vector3 transformRootPosition = transformRoot_->GetPosition();
    Vector3 transformRootRotation = transformRoot_->GetRotation().EulerAngles();

    /*ATOMIC_LOGDEBUGF("ModelPos - (%f, %f, %f)", modelPosition.x_, modelPosition.y_, modelPosition.z_);
    ATOMIC_LOGDEBUGF("ModelRot - (%f, %f, %f)", modelRotation.x_, modelRotation.y_, modelRotation.z_);

    ATOMIC_LOGDEBUGF("TransPos - (%f, %f, %f)", transformRootPosition.x_, transformRootPosition.y_, transformRootPosition.z_);
    ATOMIC_LOGDEBUGF("TransRot - (%f, %f, %f)", transformRootRotation.x_, transformRootRotation.y_, transformRootRotation.z_);

    ATOMIC_LOGDEBUGF("OffsetPos - (%f, %f, %f)", rootNodePositionOffset_.x_, rootNodePositionOffset_.y_, rootNodePositionOffset_.z_);
    ATOMIC_LOGDEBUGF("OffsetRot - (%f, %f, %f)", offsetRotation.x_, offsetRotation.y_, offsetRotation.z_);*/
}

void AnimationState::OnRemove()
{
    ResetRootMotionOffset();
}

void AnimationState::ResetRootMotionOffset()
{
    rootNodePositionOffset_ = Vector3(0.0f, 0.0f, 0.0f);
    rootNodeRotationOffset_ = Quaternion(0.0f, 0.0f, 0.0f);
}

// LUMA END

}
