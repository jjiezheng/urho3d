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
#include "Deserializer.h"
#include "Profiler.h"
#include "Serializer.h"
#include "Skeleton.h"

#include "DebugNew.h"

Skeleton::Skeleton() :
    mRootBone(0)
{
}

Skeleton::~Skeleton()
{
    clearBones();
}

void Skeleton::load(Deserializer& source)
{
    PROFILE(Skeleton_Load);
    
    clearBones();
    
    unsigned bones = source.readUInt();
    std::vector<unsigned> boneParents;
    
    // First pass: read bone data, identify root bone
    for (unsigned i = 0; i < bones; ++i)
    {
        std::string name = source.readString();
        unsigned parentIndex = source.readUInt();
        SharedPtr<Bone> newBone(new Bone(0, name));

        Vector3 initialPosition = source.readVector3();
        Quaternion initialRotation = source.readQuaternion();
        Vector3 initialScale = source.readVector3();
        Matrix4x3 offsetMatrix;
        source.read(&offsetMatrix.m00, sizeof(Matrix4x3));
        
        newBone->setInitialTransform(initialPosition, initialRotation, initialScale);
        newBone->setOffsetMatrix(offsetMatrix);
        newBone->reset();
        
        // Read bone collision data
        unsigned char collisionMask = source.readUByte();
        if (collisionMask & BONECOLLISION_SPHERE)
            newBone->setRadius(source.readFloat());
        if (collisionMask & BONECOLLISION_BOX)
            newBone->setBoundingBox(source.readBoundingBox());
        
        if (parentIndex == i)
            mRootBone = newBone;
        
        mBones.push_back(newBone);
        boneParents.push_back(parentIndex);
    }
    
    // Second pass: map parent bones & root bone
    for (unsigned i = 0; i < bones; ++i)
    {
        mBones[i]->setRootBone(mRootBone);
        unsigned parentBoneIndex = boneParents[i];
        if (parentBoneIndex != i)
        {
            if (parentBoneIndex >= mBones.size())
                EXCEPTION("Illegal parent bone assignment");
            
            mBones[parentBoneIndex]->addChild(mBones[i]);
        }
    }
}

void Skeleton::save(Serializer& dest)
{
    dest.writeUInt(mBones.size());
    for (unsigned i = 0; i < mBones.size(); ++i)
    {
        Bone* bone = mBones[i];
        
        // Bone name
        dest.writeString(bone->getName());
        
        // Parent index, same as own if root bone
        unsigned parentIndex = getBoneIndex(dynamic_cast<Bone*>(bone->getParent()));
        if (parentIndex == M_MAX_UNSIGNED)
            parentIndex = i;
        dest.writeUInt(parentIndex);
        
        // Initial position and offset matrix
        dest.writeVector3(bone->getInitialPosition());
        dest.writeQuaternion(bone->getInitialRotation());
        dest.writeVector3(bone->getInitialScale());
        dest.write(bone->getOffsetMatrix().getData(), sizeof(Matrix4x3));
        
        // Collision info
        unsigned char collisionMask = bone->getCollisionMask();
        dest.writeUByte(collisionMask);
        if (collisionMask & BONECOLLISION_SPHERE)
            dest.writeFloat(bone->getRadius());
        if (collisionMask & BONECOLLISION_BOX)
            dest.writeBoundingBox(bone->getBoundingBox());
    }
}

void Skeleton::define(const std::vector<SharedPtr<Bone > >& srcBones)
{
    clearBones();
    
    if (!srcBones.size())
        return;
    
    std::map<Bone*, unsigned> srcBoneIndices;
    Bone* srcRootBone = 0;
    
    // First pass: copy the bones
    for (unsigned i = 0; i < srcBones.size(); ++i)
    {
        srcBoneIndices[srcBones[i]] = i;
        
        SharedPtr<Bone> newBone(new Bone(0, srcBones[i]->getName()));
        newBone->setInitialTransform(srcBones[i]->getInitialPosition(), srcBones[i]->getInitialRotation(),
            srcBones[i]->getInitialScale());
        newBone->setOffsetMatrix(srcBones[i]->getOffsetMatrix());
        
        if (srcBones[i]->getCollisionMask() & BONECOLLISION_SPHERE)
            newBone->setRadius(srcBones[i]->getRadius());
        if (srcBones[i]->getCollisionMask() & BONECOLLISION_BOX)
            newBone->setBoundingBox(srcBones[i]->getBoundingBox());
        newBone->reset();
        
        Bone* srcParentBone = static_cast<Bone*>(srcBones[i]->getParent());
        // If parent bone is none of the listed bones, treat it as a root bone
        if ((!srcParentBone) || (srcBoneIndices.find(srcParentBone) == srcBoneIndices.end()))
            srcRootBone = srcBones[i];
        
        mBones.push_back(newBone);
    }
    
    // Second pass: copy the hierarchy
    unsigned rootBoneIndex = srcBoneIndices[srcRootBone];
    mRootBone = mBones[rootBoneIndex];
    
    for (unsigned i = 0; i < mBones.size(); ++i)
    {
        mBones[i]->setRootBone(mRootBone);
        
        Bone* srcParentBone = static_cast<Bone*>(srcBones[i]->getParent());
        if ((srcParentBone) && (srcBoneIndices.find(srcParentBone) != srcBoneIndices.end()))
        {
            unsigned parentBoneIndex = srcBoneIndices[srcParentBone];
            mBones[parentBoneIndex]->addChild(mBones[i]);
        }
    }
}

void Skeleton::setBones(const std::vector<SharedPtr<Bone> >& bones, Bone* rootBone)
{
    mBones = bones;
    mRootBone = rootBone;
}

void Skeleton::reset(bool force)
{
    // Start with resetting the root bone so that node dirtying is done most efficiently
    if (mRootBone)
        mRootBone->reset(force);
    // Then reset the rest of the bones
    for (std::vector<SharedPtr<Bone> >::iterator i = mBones.begin(); i != mBones.end(); ++i)
    {
        if ((*i) != mRootBone)
            (*i)->reset(force);
    }
}

Bone* Skeleton::getBone(unsigned index) const
{
    return index < mBones.size() ? mBones[index] : (Bone*)0;
}

Bone* Skeleton::getBone(const std::string& name) const
{
    for (std::vector<SharedPtr<Bone> >::const_iterator i = mBones.begin(); i != mBones.end(); ++i)
    {
        if ((*i)->getName() == name)
            return *i;
    }
    
    return 0;
}

Bone* Skeleton::getBone(StringHash nameHash) const
{
    for (std::vector<SharedPtr<Bone> >::const_iterator i = mBones.begin(); i != mBones.end(); ++i)
    {
        if ((*i)->getNameHash() == nameHash)
            return *i;
    }
    
    return 0;
}

unsigned Skeleton::getBoneIndex(Bone* bone) const
{
    for (unsigned i = 0; i < mBones.size(); ++i)
    {
        if (mBones[i] == bone)
            return i;
    }
    return M_MAX_UNSIGNED;
}

void Skeleton::clearBones()
{
    if ((mRootBone) && (mRootBone->getParent()))
        mRootBone->getParent()->removeChild(mRootBone);
    
    mBones.clear();
    mRootBone.reset();
}
