//
// Urho3D Engine
// Copyright (c) 2008-2011 Lasse ��rni
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

#include "Precompiled.h"
#include "AnimatedModel.h"
#include "Animation.h"
#include "AnimationController.h"
#include "AnimationState.h"
#include "Context.h"
#include "MemoryBuffer.h"
#include "Profiler.h"
#include "ResourceCache.h"
#include "Scene.h"
#include "SceneEvents.h"
#include "VectorBuffer.h"

#include "DebugNew.h"

static std::string noBoneName;

OBJECTTYPESTATIC(AnimationController);

AnimationController::AnimationController(Context* context) :
    Component(context)
{
}

AnimationController::~AnimationController()
{
}

void AnimationController::RegisterObject(Context* context)
{
    context->RegisterFactory<AnimationController>();
    
    ATTRIBUTE(AnimationController, VAR_BUFFER, "Animations", animations_, std::vector<unsigned char>());
}


void AnimationController::OnSetAttribute(const AttributeInfo& attr, const Variant& value)
{
    switch (attr.offset_)
    {
    case offsetof(AnimationController, animations_):
        {
            MemoryBuffer buf(value.GetBuffer());
            animations_.resize(buf.ReadVLE());
            for (std::vector<AnimationControl>::iterator i = animations_.begin(); i != animations_.end(); ++i)
            {
                i->hash_ = buf.ReadStringHash();
                i->speed_ = buf.ReadFloat();
                i->targetWeight_ = buf.ReadFloat();
                i->fadeTime_ = buf.ReadFloat();
                i->autoFadeTime_ = buf.ReadFloat();
            }
        }
        break;
        
    default:
        Serializable::OnSetAttribute(attr, value);
        break;
    }
}

Variant AnimationController::OnGetAttribute(const AttributeInfo& attr)
{
    switch (attr.offset_)
    {
    case offsetof(AnimationController, animations_):
        {
            VectorBuffer buf;
            buf.WriteVLE(animations_.size());
            for (std::vector<AnimationControl>::const_iterator i = animations_.begin(); i != animations_.end(); ++i)
            {
                buf.WriteStringHash(i->hash_);
                buf.WriteFloat(i->speed_);
                buf.WriteFloat(i->targetWeight_);
                buf.WriteFloat(i->fadeTime_);
                buf.WriteFloat(i->autoFadeTime_);
            }
            return buf.GetBuffer();
        }
        
    default:
        return Serializable::OnGetAttribute(attr);
    }
}

void AnimationController::Update(float timeStep)
{
    AnimatedModel* model = GetComponent<AnimatedModel>();
    if (!model)
        return;
    
    PROFILE(UpdateAnimationController);
    
    // Loop through animations
    for (std::vector<AnimationControl>::iterator i = animations_.begin(); i != animations_.end();)
    {
        bool remove = false;
        AnimationState* state = model->GetAnimationState(i->hash_);
        if (!state)
            remove = true;
        else
        {
            // Advance the animation
            if (i->speed_ != 0.0f)
                state->AddTime(i->speed_ * timeStep);
            
            float targetWeight = i->targetWeight_;
            float fadeTime = i->fadeTime_;
            
            // If non-looped animation at the end, activate autofade as applicable
            if ((!state->IsLooped()) && (state->GetTime() >= state->GetLength()) && (i->autoFadeTime_ > 0.0f))
            {
                targetWeight = 0.0f;
                fadeTime = i->autoFadeTime_;
            }
            
            // Process weight fade
            float currentWeight = state->GetWeight();
            if ((currentWeight != targetWeight) && (fadeTime > 0.0f))
            {
                float weightDelta = 1.0f / fadeTime * timeStep;
                if (currentWeight < targetWeight)
                    currentWeight = Min(currentWeight + weightDelta, targetWeight);
                else if (currentWeight > targetWeight)
                    currentWeight = Max(currentWeight - weightDelta, targetWeight);
                state->SetWeight(currentWeight);
            }
            
            // Remove if weight zero and target weight zero
            if ((state->GetWeight() == 0.0f) && ((targetWeight == 0.0f) || (fadeTime == 0.0f)))
                remove = true;
        }
        
        if (remove)
        {
            if (state)
                model->RemoveAnimationState(state);
            i = animations_.erase(i);
        }
        else
            ++i;
    }
}

bool AnimationController::Play(const std::string& name, int layer, bool looped, float fadeInTime)
{
    AnimatedModel* model = GetComponent<AnimatedModel>();
    if (!model)
        return false;
    
    // Check if already exists
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);
    
    if (!state)
    {
        Animation* newAnimation = GetSubsystem<ResourceCache>()->GetResource<Animation>(name);
        state = model->AddAnimationState(newAnimation);
        if (!state)
            return false;
    }
    
    if (index == M_MAX_UNSIGNED)
    {
        AnimationControl newControl;
        Animation* animation = state->GetAnimation();
        newControl.hash_ = animation->GetNameHash();
        animations_.push_back(newControl);
        index = animations_.size() - 1;
    }
    
    state->SetLayer(layer);
    state->SetLooped(looped);
    
    if (fadeInTime > 0.0f)
    {
        animations_[index].targetWeight_ = 1.0f;
        animations_[index].fadeTime_ = fadeInTime;
    }
    else
        state->SetWeight(1.0f);
    
    return true;
}

bool AnimationController::PlayExclusive(const std::string& name, int layer, bool looped, float fadeTime)
{
    FadeOthers(name, 0.0f, fadeTime);
    return Play(name, layer, looped, fadeTime);
}

bool AnimationController::Stop(const std::string& name, float fadeOutTime)
{
    AnimatedModel* model = GetComponent<AnimatedModel>();
    if (!model)
        return false;
    
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);
    if (fadeOutTime <= 0.0f)
    {
        if (index != M_MAX_UNSIGNED)
            animations_.erase(animations_.begin() + index);
        if (state)
            model->RemoveAnimationState(state);
    }
    else
    {
        if (index != M_MAX_UNSIGNED)
        {
            animations_[index].targetWeight_ = 0.0f;
            animations_[index].fadeTime_ = fadeOutTime;
        }
    }
    
    return (index != M_MAX_UNSIGNED) || (state != 0);
}

void AnimationController::StopLayer(int layer, float fadeOutTime)
{
    AnimatedModel* model = GetComponent<AnimatedModel>();
    if (!model)
        return;
    
    for (std::vector<AnimationControl>::iterator i = animations_.begin(); i != animations_.end();)
    {
        AnimationState* state = model->GetAnimationState(i->hash_);
        bool remove = false;
        
        if ((state) && (state->GetLayer() == layer))
        {
            if (fadeOutTime <= 0.0f)
            {
                remove = true;
                
                if (state)
                    model->RemoveAnimationState(state);
            }
            else
            {
                i->targetWeight_ = 0.0f;
                i->fadeTime_ = fadeOutTime;
            }
        }
        
        if (remove)
            i = animations_.erase(i);
        else
            ++i;
    }
}

void AnimationController::StopAll(float fadeOutTime)
{
    AnimatedModel* model = GetComponent<AnimatedModel>();
    if (!model)
        return;
    
    for (std::vector<AnimationControl>::iterator i = animations_.begin(); i != animations_.end();)
    {
        bool remove = false;
        
        if (fadeOutTime <= 0.0f)
        {
            remove = true;
            AnimationState* state = model->GetAnimationState(i->hash_);
            if (state)
                model->RemoveAnimationState(state);
        }
        else
        {
            i->targetWeight_ = 0.0f;
            i->fadeTime_ = fadeOutTime;
        }
    
        if (remove)
            i = animations_.erase(i);
        else
            ++i;
    }
}

bool AnimationController::Fade(const std::string& name, float targetWeight, float fadeTime)
{
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);
    if (index == M_MAX_UNSIGNED)
        return false;
    
    animations_[index].targetWeight_ = Clamp(targetWeight, 0.0f, 1.0f);
    animations_[index].fadeTime_ = Max(fadeTime, M_EPSILON);
    return true;
}

bool AnimationController::FadeOthers(const std::string& name, float targetWeight, float fadeTime)
{
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);
    if ((index == M_MAX_UNSIGNED) || (!state))
        return false;
    
    AnimatedModel* model = GetComponent<AnimatedModel>();
    int layer = state->GetLayer();
    
    for (unsigned i = 0; i < animations_.size(); ++i)
    {
        if (i != index)
        {
            AnimationControl& control = animations_[i];
            AnimationState* otherState = model->GetAnimationState(control.hash_);
            if ((otherState) && (otherState->GetLayer() == layer))
            {
                control.targetWeight_ = Clamp(targetWeight, 0.0f, 1.0f);
                control.fadeTime_ = Max(fadeTime, M_EPSILON);
            }
        }
    }
    return true;
}

bool AnimationController::SetLayer(const std::string& name, int layer)
{
    AnimationState* state = FindAnimationState(name);
    if (!state)
        return false;
    
    state->SetLayer(layer);
    return true;
}

bool AnimationController::SetStartBone(const std::string& name, const std::string& startBoneName)
{
    AnimationState* state = FindAnimationState(name);
    if (!state)
        return false;
    
    AnimatedModel* model = GetComponent<AnimatedModel>();
    Bone* bone = model->GetSkeleton().GetBone(startBoneName);
    state->SetStartBone(bone);
    return true;
}

bool AnimationController::SetTime(const std::string& name, float time)
{
    AnimationState* state = FindAnimationState(name);
    if (!state)
        return false;
    
    state->SetTime(time);
    return true;
}

bool AnimationController::SetSpeed(const std::string& name, float speed)
{
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);
    if (index == M_MAX_UNSIGNED)
        return false;
    
    animations_[index].speed_ = speed;
    return true;
}

bool AnimationController::SetWeight(const std::string& name, float weight)
{
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);
    if ((index == M_MAX_UNSIGNED) || (!state))
        return false;
    
    state->SetWeight(weight);
    // Stop any ongoing fade
    animations_[index].fadeTime_ = 0.0f;
    return true;
}

bool AnimationController::SetLooped(const std::string& name, bool enable)
{
    AnimationState* state = FindAnimationState(name);
    if (!state)
        return false;
    
    state->SetLooped(enable);
    return true;
}

bool AnimationController::SetAutoFade(const std::string& name, float fadeOutTime)
{
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);
    if (index == M_MAX_UNSIGNED)
        return false;
    
    animations_[index].autoFadeTime_ = Max(fadeOutTime, 0.0f);
    return true;
}

bool AnimationController::IsPlaying(const std::string& name) const
{
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);
    return index != M_MAX_UNSIGNED;
}

bool AnimationController::IsFadingIn(const std::string& name) const
{
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);
    if ((index == M_MAX_UNSIGNED) || (!state))
        return false;
    
    return (animations_[index].fadeTime_) && (animations_[index].targetWeight_ > state->GetWeight());
}

bool AnimationController::IsFadingOut(const std::string& name) const
{
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);
    if ((index == M_MAX_UNSIGNED) || (!state))
        return false;
    
    return ((animations_[index].fadeTime_) && (animations_[index].targetWeight_ < state->GetWeight()))
        || ((!state->IsLooped()) && (state->GetTime() >= state->GetLength()) && (animations_[index].autoFadeTime_));
}

int AnimationController::GetLayer(const std::string& name) const
{
    AnimationState* state = FindAnimationState(name);
    if (!state)
        return 0;
    return state->GetLayer();
}

Bone* AnimationController::GetStartBone(const std::string& name) const
{
    AnimationState* state = FindAnimationState(name);
    if (!state)
        return 0;
    return state->GetStartBone();
}

const std::string& AnimationController::GetStartBoneName(const std::string& name) const
{
    Bone* bone = GetStartBone(name);
    return bone ? bone->name_ : noBoneName;
}

float AnimationController::GetTime(const std::string& name) const
{
    AnimationState* state = FindAnimationState(name);
    return state ? state->GetTime() : 0.0f;
}

float AnimationController::GetWeight(const std::string& name) const
{
    AnimationState* state = FindAnimationState(name);
    return state ? state->GetWeight() : 0.0f;
}

bool AnimationController::IsLooped(const std::string& name) const
{
    AnimationState* state = FindAnimationState(name);
    return state ? state->IsLooped() : false;
}

float AnimationController::GetLength(const std::string& name) const
{
    AnimationState* state = FindAnimationState(name);
    return state ? state->GetLength() : 0.0f;
}

float AnimationController::GetSpeed(const std::string& name) const
{
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);
    if (index == M_MAX_UNSIGNED)
        return 0.0f;
    return animations_[index].speed_;
}

float AnimationController::GetFadeTarget(const std::string& name) const
{
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);
    if (index == M_MAX_UNSIGNED)
        return 0.0f;
    return animations_[index].targetWeight_;
}

float AnimationController::GetFadeTime(const std::string& name) const
{
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);
    if (index == M_MAX_UNSIGNED)
        return 0.0f;
    return animations_[index].targetWeight_;
}

float AnimationController::GetAutoFade(const std::string& name) const
{
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);
    if (index == M_MAX_UNSIGNED)
        return 0.0f;
    return animations_[index].autoFadeTime_;
}

void AnimationController::OnNodeSet(Node* node)
{
    if (node)
    {
        Scene* scene = node->GetScene();
        if (scene)
            SubscribeToEvent(scene, E_SCENEPOSTUPDATE, HANDLER(AnimationController, HandleScenePostUpdate));
    }
}

void AnimationController::FindAnimation(const std::string& name, unsigned& index, AnimationState*& state) const
{
    AnimatedModel* model = GetComponent<AnimatedModel>();
    StringHash nameHash(name);
    
    // Find the AnimationState
    state = model ? model->GetAnimationState(nameHash) : 0;
    if (state)
    {
        // Either a resource name or animation name may be specified. We store resource names, so correct the hash if necessary
        nameHash = state->GetAnimation()->GetNameHash();
    }
    
    // Find the internal control structure
    index = M_MAX_UNSIGNED;
    for (unsigned i = 0; i < animations_.size(); ++i)
    {
        if (animations_[i].hash_ == nameHash)
        {
            index = i;
            break;
        }
    }
}

AnimationState* AnimationController::FindAnimationState(const std::string& name) const
{
    AnimatedModel* model = GetComponent<AnimatedModel>();
    return model ? model->GetAnimationState(name) : 0;
}

void AnimationController::HandleScenePostUpdate(StringHash eventType, VariantMap& eventData)
{
    using namespace ScenePostUpdate;
    
    Update(eventData[P_TIMESTEP].GetFloat());
}