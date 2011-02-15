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

#ifndef MATH_RAY_H
#define MATH_RAY_H

#include "Vector3.h"

//! An infinite straight line in three-dimensional space
class Ray
{
public:
    //! Construct an undefined ray
    Ray()
    {
    }
    
    //! Construct from origin and direction. The direction must be normalized
    Ray(const Vector3& origin, const Vector3& direction) :
        mOrigin(origin),
        mDirection(direction)
    {
    }
    
    //! Copy-construct from another ray
    Ray(const Ray& ray) :
        mOrigin(ray.mOrigin),
        mDirection(ray.mDirection)
    {
    }
    
    //! Assign from another ray
    Ray& operator = (const Ray& rhs)
    {
        mOrigin = rhs.mOrigin;
        mDirection = rhs.mDirection;
        
        return *this;
    }
    
    //! Check for equality with another ray
    bool operator == (const Ray& rhs) const
    {
        return (mOrigin == rhs.mOrigin) && (mDirection == rhs.mDirection);
    }
    
    //! Check for inequality with another ray
    bool operator != (const Ray& rhs) const
    {
        return (mOrigin != rhs.mOrigin) || (mDirection != rhs.mDirection);
    }
    
    //! Define from origin and direction. The direction will be normalized
    void define(const Vector3& origin, const Vector3& direction)
    {
        mOrigin = origin;
        mDirection = direction.getNormalized();
    }
    
    //! Project a point on the ray
    Vector3 project(const Vector3& point) const;
    //! Return minimum distance to a triangle, or infinity if no hit
    float getDistance(const Vector3& v0, const Vector3& v1, const Vector3& v2) const;
    //! Return minimum distance to a triangle mesh defined by vertex and index data
    float getDistance(const void* vertexData, unsigned vertexSize, const void* indexData, unsigned indexSize, unsigned indexStart, unsigned indexCount) const;
    
    //! Ray origin
    Vector3 mOrigin;
    //! Ray direction
    Vector3 mDirection;
};

#endif // MATH_RAY_H
