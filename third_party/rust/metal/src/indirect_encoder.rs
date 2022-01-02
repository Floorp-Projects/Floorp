use super::*;

bitflags! {
    #[allow(non_upper_case_globals)]
    pub struct MTLIndirectCommandType: NSUInteger {
        const Draw                      = 1 << 0;
        const DrawIndexed               = 1 << 1;
        const DrawPatches               = 1 << 2;
        const DrawIndexedPatches        = 1 << 3;
        const ConcurrentDispatch        = 1 << 4;
        const ConcurrentDispatchThreads = 1 << 5;
    }
}

pub enum MTLIndirectCommandBufferDescriptor {}

foreign_obj_type! {
    type CType = MTLIndirectCommandBufferDescriptor;
    pub struct IndirectCommandBufferDescriptor;
    pub struct IndirectCommandBufferDescriptorRef;
}

impl IndirectCommandBufferDescriptorRef {
    pub fn command_types(&self) -> MTLIndirectCommandType {
        unsafe { msg_send![self, commandTypes] }
    }

    pub fn set_command_types(&self, types: MTLIndirectCommandType) {
        unsafe { msg_send![self, setCommandTypes: types] }
    }

    pub fn inherit_buffer(&self) -> bool {
        unsafe {
            match msg_send![self, inheritBuffer] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    pub fn set_inherit_buffer(&self, inherit: bool) {
        unsafe { msg_send![self, setInheritBuffer: inherit] }
    }

    pub fn inherit_pipeline_state(&self) -> bool {
        unsafe {
            match msg_send![self, inheritPipelineState] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    pub fn set_inherit_pipeline_state(&self, inherit: bool) {
        unsafe { msg_send![self, setInheritPipelineState: inherit] }
    }

    pub fn max_vertex_buffer_bind_count(&self) -> NSUInteger {
        unsafe { msg_send![self, maxVertexBufferBindCount] }
    }

    pub fn set_max_vertex_buffer_bind_count(&self, count: NSUInteger) {
        unsafe { msg_send![self, setMaxVertexBufferBindCount: count] }
    }

    pub fn max_fragment_buffer_bind_count(&self) -> NSUInteger {
        unsafe { msg_send![self, maxFragmentBufferBindCount] }
    }

    pub fn set_max_fragment_buffer_bind_count(&self, count: NSUInteger) {
        unsafe { msg_send![self, setMaxFragmentBufferBindCount: count] }
    }

    pub fn max_kernel_buffer_bind_count(&self) -> NSUInteger {
        unsafe { msg_send![self, maxKernelBufferBindCount] }
    }

    pub fn set_max_kernel_buffer_bind_count(&self, count: NSUInteger) {
        unsafe { msg_send![self, setMaxKernelBufferBindCount: count] }
    }
}

pub enum MTLIndirectCommandBuffer {}

foreign_obj_type! {
    type CType = MTLIndirectCommandBuffer;
    pub struct IndirectCommandBuffer;
    pub struct IndirectCommandBufferRef;
    type ParentType = ResourceRef;
}

impl IndirectCommandBufferRef {
    pub fn size(&self) -> NSUInteger {
        unsafe { msg_send![self, size] }
    }

    pub fn indirect_render_command_at_index(&self, index: NSUInteger) -> &IndirectRenderCommandRef {
        unsafe { msg_send![self, indirectRenderCommandAtIndex: index] }
    }

    pub fn indirect_compute_command_at_index(
        &self,
        index: NSUInteger,
    ) -> &IndirectComputeCommandRef {
        unsafe { msg_send![self, indirectComputeCommandAtIndex: index] }
    }

    pub fn reset_with_range(&self, range: crate::NSRange) {
        unsafe { msg_send![self, resetWithRange: range] }
    }
}

pub enum MTLIndirectRenderCommand {}

foreign_obj_type! {
    type CType = MTLIndirectRenderCommand;
    pub struct IndirectRenderCommand;
    pub struct IndirectRenderCommandRef;
}

impl IndirectRenderCommandRef {
    pub fn set_render_pipeline_state(&self, pipeline_state: &RenderPipelineStateRef) {
        unsafe { msg_send![self, setRenderPipelineState: pipeline_state] }
    }

    pub fn set_vertex_buffer(
        &self,
        index: NSUInteger,
        buffer: Option<&BufferRef>,
        offset: NSUInteger,
    ) {
        unsafe {
            msg_send![self,
                setVertexBuffer: buffer
                offset: offset
                atIndex: index
            ]
        }
    }

    pub fn set_fragment_buffer(
        &self,
        index: NSUInteger,
        buffer: Option<&BufferRef>,
        offset: NSUInteger,
    ) {
        unsafe {
            msg_send![self,
                setFragmentBuffer:buffer
                offset:offset
                atIndex:index
            ]
        }
    }

    pub fn draw_primitives(
        &self,
        primitive_type: MTLPrimitiveType,
        vertex_start: NSUInteger,
        vertex_count: NSUInteger,
        instance_count: NSUInteger,
        base_instance: NSUInteger,
    ) {
        unsafe {
            msg_send![self,
                drawPrimitives: primitive_type
                vertexStart: vertex_start
                vertexCount: vertex_count
                instanceCount: instance_count
                baseInstance: base_instance
            ]
        }
    }

    pub fn draw_indexed_primitives(
        &self,
        primitive_type: MTLPrimitiveType,
        index_count: NSUInteger,
        index_type: MTLIndexType,
        index_buffer: &BufferRef,
        index_buffer_offset: NSUInteger,
        instance_count: NSUInteger,
        base_vertex: NSUInteger,
        base_instance: NSUInteger,
    ) {
        unsafe {
            msg_send![self,
                drawIndexedPrimitives: primitive_type
                indexCount: index_count
                indexType: index_type
                indexBuffer: index_buffer
                indexBufferOffset: index_buffer_offset
                instanceCount: instance_count
                baseVertex: base_vertex
                baseInstance: base_instance
            ]
        }
    }

    pub fn draw_patches(
        &self,
        number_of_patch_control_points: NSUInteger,
        patch_start: NSUInteger,
        patch_count: NSUInteger,
        patch_index_buffer: &BufferRef,
        patch_index_buffer_offset: NSUInteger,
        instance_count: NSUInteger,
        base_instance: NSUInteger,
        tesselation_factor_buffer: &BufferRef,
        tesselation_factor_buffer_offset: NSUInteger,
        tesselation_factor_buffer_instance_stride: NSUInteger,
    ) {
        unsafe {
            msg_send![self,
                drawPatches: number_of_patch_control_points
                patchStart: patch_start
                patchCount: patch_count
                patchIndexBuffer: patch_index_buffer
                patchIndexBufferOffset: patch_index_buffer_offset
                instanceCount: instance_count
                baseInstance: base_instance
                tessellationFactorBuffer: tesselation_factor_buffer
                tessellationFactorBufferOffset: tesselation_factor_buffer_offset
                tessellationFactorBufferInstanceStride: tesselation_factor_buffer_instance_stride
            ]
        }
    }

    pub fn draw_indexed_patches(
        &self,
        number_of_patch_control_points: NSUInteger,
        patch_start: NSUInteger,
        patch_count: NSUInteger,
        patch_index_buffer: &BufferRef,
        patch_index_buffer_offset: NSUInteger,
        control_point_index_buffer: &BufferRef,
        control_point_index_buffer_offset: NSUInteger,
        instance_count: NSUInteger,
        base_instance: NSUInteger,
        tesselation_factor_buffer: &BufferRef,
        tesselation_factor_buffer_offset: NSUInteger,
        tesselation_factor_buffer_instance_stride: NSUInteger,
    ) {
        unsafe {
            msg_send![self,
                drawIndexedPatches: number_of_patch_control_points
                patchStart: patch_start
                patchCount: patch_count
                patchIndexBuffer: patch_index_buffer
                patchIndexBufferOffset: patch_index_buffer_offset
                controlPointIndexBuffer: control_point_index_buffer
                controlPointIndexBufferOffset: control_point_index_buffer_offset
                instanceCount: instance_count
                baseInstance: base_instance
                tessellationFactorBuffer: tesselation_factor_buffer
                tessellationFactorBufferOffset: tesselation_factor_buffer_offset
                tessellationFactorBufferInstanceStride: tesselation_factor_buffer_instance_stride
            ]
        }
    }

    pub fn reset(&self) {
        unsafe { msg_send![self, reset] }
    }
}

pub enum MTLIndirectComputeCommand {}

foreign_obj_type! {
    type CType = MTLIndirectComputeCommand;
    pub struct IndirectComputeCommand;
    pub struct IndirectComputeCommandRef;
}

impl IndirectComputeCommandRef {
    pub fn set_compute_pipeline_state(&self, state: &ComputePipelineStateRef) {
        unsafe { msg_send![self, setComputePipelineState: state] }
    }

    pub fn set_kernel_buffer(
        &self,
        index: NSUInteger,
        buffer: Option<&BufferRef>,
        offset: NSUInteger,
    ) {
        unsafe {
            msg_send![self,
                setKernelBuffer: buffer
                offset: offset
                atIndex: index
            ]
        }
    }

    pub fn set_threadgroup_memory_length(&self, index: NSUInteger, length: NSUInteger) {
        unsafe {
            msg_send![self,
                setThreadgroupMemoryLength: length
                atIndex: index
            ]
        }
    }

    pub fn set_stage_in_region(&self, region: MTLRegion) {
        unsafe { msg_send![self, setStageInRegion: region] }
    }

    pub fn set_barrier(&self) {
        unsafe { msg_send![self, setBarrier] }
    }

    pub fn clear_barrier(&self) {
        unsafe { msg_send![self, clearBarrier] }
    }

    pub fn concurrent_dispatch_threadgroups(
        &self,
        thread_groups_per_grid: MTLSize,
        threads_per_threadgroup: MTLSize,
    ) {
        unsafe {
            msg_send![self,
                concurrentDispatchThreadgroups: thread_groups_per_grid
                threadsPerThreadgroup: threads_per_threadgroup
            ]
        }
    }

    pub fn concurrent_dispatch_threads(
        &self,
        thread_groups_per_grid: MTLSize,
        threads_per_threadgroup: MTLSize,
    ) {
        unsafe {
            msg_send![self,
                concurrentDispatchThreads: thread_groups_per_grid
                threadsPerThreadgroup: threads_per_threadgroup
            ]
        }
    }

    pub fn reset(&self) {
        unsafe { msg_send![self, reset] }
    }
}
