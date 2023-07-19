// Copyright 2023 GFX developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

use super::*;

bitflags! {
    #[derive(Copy, Clone, Debug, Default, Hash, PartialEq, Eq, PartialOrd, Ord)]
    pub struct MTLAccelerationStructureInstanceOptions: u32 {
        const None = 0;
        const DisableTriangleCulling = (1 << 0);
        const TriangleFrontFacingWindingCounterClockwise = (1 << 1);
        const Opaque = (1 << 2);
        const NonOpaque = (1 << 3);
    }
}

/// See <https://developer.apple.com/documentation/metal/mtlaccelerationstructureinstancedescriptortype>
#[repr(u64)]
#[allow(non_camel_case_types)]
#[derive(Copy, Clone, Debug, Hash, PartialEq, Eq)]
pub enum MTLAccelerationStructureInstanceDescriptorType {
    Default = 0,
    UserID = 1,
    Motion = 2,
}

#[derive(Clone, Copy, PartialEq, Debug, Default)]
#[repr(C)]
pub struct MTLAccelerationStructureInstanceDescriptor {
    pub transformation_matrix: [[f32; 3]; 4],
    pub options: MTLAccelerationStructureInstanceOptions,
    pub mask: u32,
    pub intersection_function_table_offset: u32,
    pub acceleration_structure_index: u32,
}

#[derive(Clone, Copy, PartialEq, Debug, Default)]
#[repr(C)]
pub struct MTLAccelerationStructureUserIDInstanceDescriptor {
    pub transformation_matrix: [[f32; 3]; 4],
    pub options: MTLAccelerationStructureInstanceOptions,
    pub mask: u32,
    pub intersection_function_table_offset: u32,
    pub acceleration_structure_index: u32,
    pub user_id: u32,
}

pub enum MTLAccelerationStructureDescriptor {}

foreign_obj_type! {
    type CType = MTLAccelerationStructureDescriptor;
    pub struct AccelerationStructureDescriptor;
    type ParentType = NsObject;
}

pub enum MTLPrimitiveAccelerationStructureDescriptor {}

foreign_obj_type! {
    type CType = MTLPrimitiveAccelerationStructureDescriptor;
    pub struct PrimitiveAccelerationStructureDescriptor;
    type ParentType = AccelerationStructureDescriptor;
}

impl PrimitiveAccelerationStructureDescriptor {
    pub fn descriptor() -> Self {
        unsafe {
            let class = class!(MTLPrimitiveAccelerationStructureDescriptor);
            msg_send![class, descriptor]
        }
    }
}

impl PrimitiveAccelerationStructureDescriptorRef {
    pub fn set_geometry_descriptors(
        &self,
        descriptors: &ArrayRef<AccelerationStructureGeometryDescriptor>,
    ) {
        unsafe { msg_send![self, setGeometryDescriptors: descriptors] }
    }
}

pub enum MTLAccelerationStructure {}

foreign_obj_type! {
    type CType = MTLAccelerationStructure;
    pub struct AccelerationStructure;
    type ParentType = Resource;
}

pub enum MTLAccelerationStructureGeometryDescriptor {}

foreign_obj_type! {
    type CType = MTLAccelerationStructureGeometryDescriptor;
    pub struct AccelerationStructureGeometryDescriptor;
    type ParentType = NsObject;
}

impl AccelerationStructureGeometryDescriptorRef {
    pub fn set_opaque(&self, opaque: bool) {
        unsafe { msg_send![self, setOpaque: opaque] }
    }
    pub fn set_primitive_data_buffer(&self, buffer: Option<&BufferRef>) {
        unsafe { msg_send![self, setPrimitiveDataBuffer: buffer] }
    }

    pub fn set_primitive_data_stride(&self, stride: NSUInteger) {
        unsafe { msg_send![self, setPrimitiveDataStride: stride] }
    }

    pub fn set_primitive_data_element_size(&self, size: NSUInteger) {
        unsafe { msg_send![self, setPrimitiveDataElementSize: size] }
    }

    pub fn set_intersection_function_table_offset(&self, offset: NSUInteger) {
        unsafe { msg_send![self, setIntersectionFunctionTableOffset: offset] }
    }
}

pub enum MTLAccelerationStructureTriangleGeometryDescriptor {}

foreign_obj_type! {
    type CType = MTLAccelerationStructureTriangleGeometryDescriptor;
    pub struct AccelerationStructureTriangleGeometryDescriptor;
    type ParentType = AccelerationStructureGeometryDescriptor;
}

impl AccelerationStructureTriangleGeometryDescriptor {
    pub fn descriptor() -> Self {
        unsafe {
            let class = class!(MTLAccelerationStructureTriangleGeometryDescriptor);
            msg_send![class, descriptor]
        }
    }
}

impl AccelerationStructureTriangleGeometryDescriptorRef {
    pub fn set_index_buffer(&self, buffer: Option<&BufferRef>) {
        unsafe { msg_send![self, setIndexBuffer: buffer] }
    }

    pub fn set_index_buffer_offset(&self, offset: NSUInteger) {
        unsafe { msg_send![self, setIndexBufferOffset: offset] }
    }

    pub fn set_index_type(&self, t: MTLIndexType) {
        unsafe { msg_send![self, setIndexType: t] }
    }

    pub fn set_vertex_buffer(&self, buffer: Option<&BufferRef>) {
        unsafe { msg_send![self, setVertexBuffer: buffer] }
    }

    pub fn set_vertex_buffer_offset(&self, offset: NSUInteger) {
        unsafe { msg_send![self, setVertexBufferOffset: offset] }
    }

    pub fn set_vertex_stride(&self, stride: NSUInteger) {
        unsafe { msg_send![self, setVertexStride: stride] }
    }

    pub fn set_triangle_count(&self, count: NSUInteger) {
        unsafe { msg_send![self, setTriangleCount: count] }
    }

    pub fn set_vertex_format(&self, format: MTLAttributeFormat) {
        unsafe { msg_send![self, setVertexFormat: format] }
    }

    pub fn set_transformation_matrix_buffer(&self, buffer: Option<&BufferRef>) {
        unsafe { msg_send![self, setTransformationMatrixBuffer: buffer] }
    }

    pub fn set_transformation_matrix_buffer_offset(&self, offset: NSUInteger) {
        unsafe { msg_send![self, setTransformationMatrixBufferOffset: offset] }
    }
}

pub enum MTLAccelerationStructureBoundingBoxGeometryDescriptor {}

foreign_obj_type! {
    type CType = MTLAccelerationStructureBoundingBoxGeometryDescriptor;
    pub struct AccelerationStructureBoundingBoxGeometryDescriptor;
    type ParentType = AccelerationStructureGeometryDescriptor;
}

impl AccelerationStructureBoundingBoxGeometryDescriptor {
    pub fn descriptor() -> Self {
        unsafe {
            let class = class!(MTLAccelerationStructureBoundingBoxGeometryDescriptor);
            msg_send![class, descriptor]
        }
    }
}

impl AccelerationStructureBoundingBoxGeometryDescriptorRef {
    pub fn set_bounding_box_buffer(&self, buffer: Option<&BufferRef>) {
        unsafe { msg_send![self, setBoundingBoxBuffer: buffer] }
    }

    pub fn set_bounding_box_count(&self, count: NSUInteger) {
        unsafe { msg_send![self, setBoundingBoxCount: count] }
    }
}

pub enum MTLInstanceAccelerationStructureDescriptor {}

foreign_obj_type! {
    type CType = MTLInstanceAccelerationStructureDescriptor;
    pub struct InstanceAccelerationStructureDescriptor;
    type ParentType = AccelerationStructureDescriptor;
}

impl InstanceAccelerationStructureDescriptor {
    pub fn descriptor() -> Self {
        unsafe {
            let class = class!(MTLInstanceAccelerationStructureDescriptor);
            msg_send![class, descriptor]
        }
    }
}

impl InstanceAccelerationStructureDescriptorRef {
    pub fn set_instance_descriptor_type(&self, ty: MTLAccelerationStructureInstanceDescriptorType) {
        unsafe { msg_send![self, setInstanceDescriptorType: ty] }
    }

    pub fn set_instanced_acceleration_structures(
        &self,
        instances: &ArrayRef<AccelerationStructure>,
    ) {
        unsafe { msg_send![self, setInstancedAccelerationStructures: instances] }
    }

    pub fn set_instance_count(&self, count: NSUInteger) {
        unsafe { msg_send![self, setInstanceCount: count] }
    }

    pub fn set_instance_descriptor_buffer(&self, buffer: &BufferRef) {
        unsafe { msg_send![self, setInstanceDescriptorBuffer: buffer] }
    }

    pub fn set_instance_descriptor_buffer_offset(&self, offset: NSUInteger) {
        unsafe { msg_send![self, setInstanceDescriptorBufferOffset: offset] }
    }

    pub fn set_instance_descriptor_stride(&self, stride: NSUInteger) {
        unsafe { msg_send![self, setInstanceDescriptorStride: stride] }
    }
}

pub enum MTLAccelerationStructureCommandEncoder {}

foreign_obj_type! {
    type CType = MTLAccelerationStructureCommandEncoder;
    pub struct  AccelerationStructureCommandEncoder;
    type ParentType = CommandEncoder;
}

impl AccelerationStructureCommandEncoderRef {
    pub fn build_acceleration_structure(
        &self,
        acceleration_structure: &self::AccelerationStructureRef,
        descriptor: &self::AccelerationStructureDescriptorRef,
        scratch_buffer: &BufferRef,
        scratch_buffer_offset: NSUInteger,
    ) {
        unsafe {
            msg_send![
            self,
            buildAccelerationStructure: acceleration_structure
            descriptor: descriptor
            scratchBuffer: scratch_buffer
            scratchBufferOffset: scratch_buffer_offset]
        }
    }

    pub fn write_compacted_acceleration_structure_size(
        &self,
        acceleration_structure: &AccelerationStructureRef,
        to_buffer: &BufferRef,
        offset: NSUInteger,
    ) {
        unsafe {
            msg_send![
                self,
                writeCompactedAccelerationStructureSize: acceleration_structure
                toBuffer: to_buffer
                offset: offset
            ]
        }
    }

    pub fn copy_and_compact_acceleration_structure(
        &self,
        source: &AccelerationStructureRef,
        destination: &AccelerationStructureRef,
    ) {
        unsafe {
            msg_send![
                self,
                copyAndCompactAccelerationStructure: source
                toAccelerationStructure: destination
            ]
        }
    }
}

pub enum MTLIntersectionFunctionTableDescriptor {}

foreign_obj_type! {
    type CType = MTLIntersectionFunctionTableDescriptor;
    pub struct IntersectionFunctionTableDescriptor;
    type ParentType = NsObject;
}

impl IntersectionFunctionTableDescriptor {
    pub fn new() -> Self {
        unsafe {
            let class = class!(MTLIntersectionFunctionTableDescriptor);
            let this: *mut <Self as ForeignType>::CType = msg_send![class, alloc];
            msg_send![this, init]
        }
    }
}

impl IntersectionFunctionTableDescriptorRef {
    pub fn set_function_count(&self, count: NSUInteger) {
        unsafe { msg_send![self, setFunctionCount: count] }
    }
}

pub enum MTLIntersectionFunctionTable {}

foreign_obj_type! {
    type CType = MTLIntersectionFunctionTable;
    pub struct IntersectionFunctionTable;
    type ParentType = Resource;
}

impl IntersectionFunctionTableRef {
    pub fn set_function(&self, function: &FunctionHandleRef, index: NSUInteger) {
        unsafe { msg_send![self, setFunction: function atIndex: index] }
    }
}
