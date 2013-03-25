//
// Copyright (c) 2008-2013 the Urho3D project.
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
#include "Batch.h"
#include "Camera.h"
#include "Context.h"
#include "Geometry.h"
#include "Log.h"
#include "Material.h"
#include "Model.h"
#include "OcclusionBuffer.h"
#include "OctreeQuery.h"
#include "Profiler.h"
#include "ResourceCache.h"
#include "ResourceEvents.h"

#include "DebugNew.h"

namespace Urho3D
{

OBJECTTYPESTATIC(StaticModel);

StaticModel::StaticModel(Context* context) :
    Drawable(context, DRAWABLE_GEOMETRY),
    occlusionLodLevel_(M_MAX_UNSIGNED),
    materialsAttr_(Material::GetTypeStatic())
{
}

StaticModel::~StaticModel()
{
}

void StaticModel::RegisterObject(Context* context)
{
    context->RegisterFactory<StaticModel>();
    
    ACCESSOR_ATTRIBUTE(StaticModel, VAR_BOOL, "Is Enabled", IsEnabled, SetEnabled, bool, true, AM_DEFAULT);
    ACCESSOR_ATTRIBUTE(StaticModel, VAR_RESOURCEREF, "Model", GetModelAttr, SetModelAttr, ResourceRef, ResourceRef(Model::GetTypeStatic()), AM_DEFAULT);
    REF_ACCESSOR_ATTRIBUTE(StaticModel, VAR_RESOURCEREFLIST, "Material", GetMaterialsAttr, SetMaterialsAttr, ResourceRefList, ResourceRefList(Material::GetTypeStatic()), AM_DEFAULT);
    ATTRIBUTE(StaticModel, VAR_BOOL, "Is Occluder", occluder_, false, AM_DEFAULT);
    ACCESSOR_ATTRIBUTE(StaticModel, VAR_BOOL, "Can Be Occluded", IsOccludee, SetOccludee, bool, true, AM_DEFAULT);
    ATTRIBUTE(StaticModel, VAR_BOOL, "Cast Shadows", castShadows_, false, AM_DEFAULT);
    ACCESSOR_ATTRIBUTE(StaticModel, VAR_FLOAT, "Draw Distance", GetDrawDistance, SetDrawDistance, float, 0.0f, AM_DEFAULT);
    ACCESSOR_ATTRIBUTE(StaticModel, VAR_FLOAT, "Shadow Distance", GetShadowDistance, SetShadowDistance, float, 0.0f, AM_DEFAULT);
    ACCESSOR_ATTRIBUTE(StaticModel, VAR_FLOAT, "LOD Bias", GetLodBias, SetLodBias, float, 1.0f, AM_DEFAULT);
    COPY_BASE_ATTRIBUTES(StaticModel, Drawable);
    ATTRIBUTE(StaticModel, VAR_INT, "Occlusion LOD Level", occlusionLodLevel_, M_MAX_UNSIGNED, AM_DEFAULT);
}

void StaticModel::ProcessRayQuery(const RayOctreeQuery& query, PODVector<RayQueryResult>& results)
{
    RayQueryLevel level = query.level_;
    
    switch (level)
    {
    case RAY_AABB_NOSUBOBJECTS:
    case RAY_AABB:
        Drawable::ProcessRayQuery(query, results);
        break;
        
    case RAY_OBB:
    case RAY_TRIANGLE:
        Matrix3x4 inverse(node_->GetWorldTransform().Inverse());
        Ray localRay(inverse * query.ray_.origin_, inverse * Vector4(query.ray_.direction_, 0.0f));
        float distance = localRay.HitDistance(boundingBox_);
        if (distance <= query.maxDistance_)
        {
            if (level == RAY_TRIANGLE)
            {
                for (unsigned i = 0; i < batches_.Size(); ++i)
                {
                    Geometry* geometry = batches_[i].geometry_;
                    if (geometry)
                    {
                        distance = geometry->GetHitDistance(localRay);
                        if (distance <= query.maxDistance_)
                        {
                            RayQueryResult result;
                            result.drawable_ = this;
                            result.node_ = node_;
                            result.distance_ = distance;
                            result.subObject_ = M_MAX_UNSIGNED;
                            results.Push(result);
                            break;
                        }
                    }
                }
            }
            else
            {
                RayQueryResult result;
                result.drawable_ = this;
                result.node_ = node_;
                result.distance_ = distance;
                result.subObject_ = M_MAX_UNSIGNED;
                results.Push(result);
            }
        }
        break;
    }
}

void StaticModel::UpdateBatches(const FrameInfo& frame)
{
    const Matrix3x4& worldTransform = node_->GetWorldTransform();
    distance_ = frame.camera_->GetDistance(GetWorldBoundingBox().Center());
    
    if (batches_.Size() > 1)
    {
        for (unsigned i = 0; i < batches_.Size(); ++i)
        {
            batches_[i].distance_ = frame.camera_->GetDistance(worldTransform * geometryData_[i].center_);
            batches_[i].worldTransform_ = &worldTransform;
        }
    }
    else if (batches_.Size() == 1)
    {
        batches_[0].distance_ = distance_;
        batches_[0].worldTransform_ = &worldTransform;
    }
    
    float scale = GetWorldBoundingBox().Size().DotProduct(DOT_SCALE);
    float newLodDistance = frame.camera_->GetLodDistance(distance_, scale, lodBias_);
    
    if (newLodDistance != lodDistance_)
    {
        lodDistance_ = newLodDistance;
        CalculateLodLevels();
    }
}

Geometry* StaticModel::GetLodGeometry(unsigned batchIndex, unsigned level)
{
    if (batchIndex >= geometries_.Size())
        return 0;
    
    // If level is out of range, use visible geometry
    if (level < geometries_[batchIndex].Size())
        return geometries_[batchIndex][level];
    else
        return batches_[batchIndex].geometry_;
}

unsigned StaticModel::GetNumOccluderTriangles()
{
    unsigned triangles = 0;
    
    for (unsigned i = 0; i < batches_.Size(); ++i)
    {
        Geometry* geometry = GetLodGeometry(i, occlusionLodLevel_);
        if (!geometry)
            continue;
        
        // Check that the material is suitable for occlusion (default material always is)
        Material* mat = batches_[i].material_;
        if (mat && !mat->GetOcclusion())
            continue;
        
        triangles += geometry->GetIndexCount() / 3;
    }
    
    return triangles;
}

bool StaticModel::DrawOcclusion(OcclusionBuffer* buffer)
{
    for (unsigned i = 0; i < batches_.Size(); ++i)
    {
        Geometry* geometry = GetLodGeometry(i, occlusionLodLevel_);
        if (!geometry)
            continue;
        
        // Check that the material is suitable for occlusion (default material always is) and set culling mode
        Material* material = batches_[i].material_;
        if (material)
        {
            if (!material->GetOcclusion())
                continue;
            buffer->SetCullMode(material->GetCullMode());
        }
        else
            buffer->SetCullMode(CULL_CCW);
        
        const unsigned char* vertexData;
        unsigned vertexSize;
        const unsigned char* indexData;
        unsigned indexSize;
        unsigned elementMask;
        
        geometry->GetRawData(vertexData, vertexSize, indexData, indexSize, elementMask);
        // Check for valid geometry data
        if (!vertexData || !indexData)
            continue;
        
        unsigned indexStart = geometry->GetIndexStart();
        unsigned indexCount = geometry->GetIndexCount();
        
        // Draw and check for running out of triangles
        if (!buffer->Draw(node_->GetWorldTransform(), vertexData, vertexSize, indexData, indexSize, indexStart, indexCount))
            return false;
    }
    
    return true;
}

void StaticModel::SetModel(Model* model)
{
    if (!model || model == model_)
        return;
    
    // If script erroneously calls StaticModel::SetModel on an AnimatedModel, warn and redirect
    if (GetType() == AnimatedModel::GetTypeStatic())
    {
        LOGWARNING("StaticModel::SetModel() called on AnimatedModel. Redirecting to AnimatedModel::SetModel()");
        AnimatedModel* animatedModel = static_cast<AnimatedModel*>(this);
        animatedModel->SetModel(model);
        return;
    }
    
    // Unsubscribe from the reload event of previous model (if any), then subscribe to the new
    if (model_)
        UnsubscribeFromEvent(model_, E_RELOADFINISHED);
    if (model)
        SubscribeToEvent(model, E_RELOADFINISHED, HANDLER(StaticModel, HandleModelReloadFinished));
    
    model_ = model;
    
    // Copy the subgeometry & LOD level structure
    SetNumGeometries(model->GetNumGeometries());
    const Vector<Vector<SharedPtr<Geometry> > >& geometries = model->GetGeometries();
    const PODVector<Vector3>& geometryCenters = model->GetGeometryCenters();
    for (unsigned i = 0; i < geometries.Size(); ++i)
    {
        geometries_[i] = geometries[i];
        geometryData_[i].center_ = geometryCenters[i];
    }
    
    SetBoundingBox(model->GetBoundingBox());
    ResetLodLevels();
    MarkNetworkUpdate();
}

void StaticModel::SetMaterial(Material* material)
{
    for (unsigned i = 0; i < batches_.Size(); ++i)
        batches_[i].material_ = material;
    
    MarkNetworkUpdate();
}

bool StaticModel::SetMaterial(unsigned index, Material* material)
{
    if (index >= batches_.Size())
    {
        LOGERROR("Material index out of bounds");
        return false;
    }
    
    batches_[index].material_ = material;
    MarkNetworkUpdate();
    return true;
}

void StaticModel::SetOcclusionLodLevel(unsigned level)
{
    occlusionLodLevel_ = level;
    MarkNetworkUpdate();
}

Material* StaticModel::GetMaterial(unsigned index) const
{
    return index < batches_.Size() ? batches_[index].material_ : (Material*)0;
}

void StaticModel::SetBoundingBox(const BoundingBox& box)
{
    boundingBox_ = box;
    OnMarkedDirty(node_);
}

void StaticModel::SetNumGeometries(unsigned num)
{
    batches_.Resize(num);
    geometries_.Resize(num);
    geometryData_.Resize(num);
    ResetLodLevels();
}

void StaticModel::SetModelAttr(ResourceRef value)
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    SetModel(cache->GetResource<Model>(value.id_));
}

void StaticModel::SetMaterialsAttr(const ResourceRefList& value)
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    for (unsigned i = 0; i < value.ids_.Size(); ++i)
        SetMaterial(i, cache->GetResource<Material>(value.ids_[i]));
}

ResourceRef StaticModel::GetModelAttr() const
{
    return GetResourceRef(model_, Model::GetTypeStatic());
}

const ResourceRefList& StaticModel::GetMaterialsAttr() const
{
    materialsAttr_.ids_.Resize(batches_.Size());
    for (unsigned i = 0; i < batches_.Size(); ++i)
        materialsAttr_.ids_[i] = batches_[i].material_ ? batches_[i].material_->GetNameHash() : StringHash();
    
    return materialsAttr_;
}

void StaticModel::OnWorldBoundingBoxUpdate()
{
    worldBoundingBox_ = boundingBox_.Transformed(node_->GetWorldTransform());
}

void StaticModel::ResetLodLevels()
{
    // Ensure that each subgeometry has at least one LOD level, and reset the current LOD level
    for (unsigned i = 0; i < batches_.Size(); ++i)
    {
        if (!geometries_[i].Size())
            geometries_[i].Resize(1);
        batches_[i].geometry_ = geometries_[i][0];
        geometryData_[i].lodLevel_ = 0;
    }
    
    // Find out the real LOD levels on next geometry update
    lodDistance_ = M_INFINITY;
}

void StaticModel::CalculateLodLevels()
{
    for (unsigned i = 0; i < batches_.Size(); ++i)
    {
        const Vector<SharedPtr<Geometry> >& batchGeometries = geometries_[i];
        unsigned j;
        
        for (j = 1; j < batchGeometries.Size(); ++j)
        {
            if (batchGeometries[j] && lodDistance_ <= batchGeometries[j]->GetLodDistance())
                break;
        }
        
        unsigned newLodLevel = j - 1;
        if (geometryData_[i].lodLevel_ != newLodLevel)
        {
            geometryData_[i].lodLevel_ = newLodLevel;
            batches_[i].geometry_ = batchGeometries[newLodLevel];
        }
    }
}

void StaticModel::HandleModelReloadFinished(StringHash eventType, VariantMap& eventData)
{
    Model* currentModel = model_;
    model_.Reset(); // Set null to allow to be re-set
    SetModel(currentModel);
}

}
