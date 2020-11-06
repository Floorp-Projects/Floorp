//! Dummy backend implementation to test the code for compile errors
//! outside of the graphics development environment.

extern crate gfx_hal as hal;

use hal::{
    adapter,
    buffer,
    command,
    device,
    format,
    image,
    memory,
    pass,
    pool,
    pso,
    query,
    queue,
    window,
};
use std::borrow::Borrow;
use std::ops::Range;

const DO_NOT_USE_MESSAGE: &str = "You need to enable a native API feature (vulkan/metal/dx11/dx12/gl/wgl) in order to use gfx-rs";

/// Dummy backend.
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
pub enum Backend {}
impl hal::Backend for Backend {
    type Instance = Instance;
    type PhysicalDevice = PhysicalDevice;
    type Device = Device;

    type Surface = Surface;
    type Swapchain = Swapchain;

    type QueueFamily = QueueFamily;
    type CommandQueue = CommandQueue;
    type CommandBuffer = CommandBuffer;

    type Memory = ();
    type CommandPool = CommandPool;

    type ShaderModule = ();
    type RenderPass = ();
    type Framebuffer = ();

    type Buffer = ();
    type BufferView = ();
    type Image = ();
    type ImageView = ();
    type Sampler = ();

    type ComputePipeline = ();
    type GraphicsPipeline = ();
    type PipelineCache = ();
    type PipelineLayout = ();
    type DescriptorSetLayout = ();
    type DescriptorPool = DescriptorPool;
    type DescriptorSet = ();

    type Fence = ();
    type Semaphore = ();
    type Event = ();
    type QueryPool = ();
}

/// Dummy physical device.
#[derive(Debug)]
pub struct PhysicalDevice;
impl adapter::PhysicalDevice<Backend> for PhysicalDevice {
    unsafe fn open(
        &self,
        _: &[(&QueueFamily, &[queue::QueuePriority])],
        _: hal::Features,
    ) -> Result<adapter::Gpu<Backend>, device::CreationError> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    fn format_properties(&self, _: Option<format::Format>) -> format::Properties {
        panic!(DO_NOT_USE_MESSAGE)
    }

    fn image_format_properties(
        &self,
        _: format::Format,
        _dim: u8,
        _: image::Tiling,
        _: image::Usage,
        _: image::ViewCapabilities,
    ) -> Option<image::FormatProperties> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    fn memory_properties(&self) -> adapter::MemoryProperties {
        panic!(DO_NOT_USE_MESSAGE)
    }

    fn features(&self) -> hal::Features {
        panic!(DO_NOT_USE_MESSAGE)
    }

    fn hints(&self) -> hal::Hints {
        panic!(DO_NOT_USE_MESSAGE)
    }

    fn limits(&self) -> hal::Limits {
        panic!(DO_NOT_USE_MESSAGE)
    }
}

/// Dummy command queue doing nothing.
#[derive(Debug)]
pub struct CommandQueue;
impl queue::CommandQueue<Backend> for CommandQueue {
    unsafe fn submit<'a, T, Ic, S, Iw, Is>(
        &mut self,
        _: queue::Submission<Ic, Iw, Is>,
        _: Option<&()>,
    ) where
        T: 'a + Borrow<CommandBuffer>,
        Ic: IntoIterator<Item = &'a T>,
        S: 'a + Borrow<()>,
        Iw: IntoIterator<Item = (&'a S, pso::PipelineStage)>,
        Is: IntoIterator<Item = &'a S>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn present<'a, W, Is, S, Iw>(
        &mut self,
        _: Is,
        _: Iw,
    ) -> Result<Option<window::Suboptimal>, window::PresentError>
    where
        W: 'a + Borrow<Swapchain>,
        Is: IntoIterator<Item = (&'a W, window::SwapImageIndex)>,
        S: 'a + Borrow<()>,
        Iw: IntoIterator<Item = &'a S>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn present_surface(
        &mut self,
        _surface: &mut Surface,
        _image: (),
        _wait_semaphore: Option<&()>,
    ) -> Result<Option<window::Suboptimal>, window::PresentError> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    fn wait_idle(&self) -> Result<(), device::OutOfMemory> {
        panic!(DO_NOT_USE_MESSAGE)
    }
}

/// Dummy device doing nothing.
#[derive(Debug)]
pub struct Device;
impl device::Device<Backend> for Device {
    unsafe fn create_command_pool(
        &self,
        _: queue::QueueFamilyId,
        _: pool::CommandPoolCreateFlags,
    ) -> Result<CommandPool, device::OutOfMemory> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn destroy_command_pool(&self, _: CommandPool) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn allocate_memory(
        &self,
        _: hal::MemoryTypeId,
        _: u64,
    ) -> Result<(), device::AllocationError> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn create_render_pass<'a, IA, IS, ID>(
        &self,
        _: IA,
        _: IS,
        _: ID,
    ) -> Result<(), device::OutOfMemory>
    where
        IA: IntoIterator,
        IA::Item: Borrow<pass::Attachment>,
        IS: IntoIterator,
        IS::Item: Borrow<pass::SubpassDesc<'a>>,
        ID: IntoIterator,
        ID::Item: Borrow<pass::SubpassDependency>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn create_pipeline_layout<IS, IR>(&self, _: IS, _: IR) -> Result<(), device::OutOfMemory>
    where
        IS: IntoIterator,
        IS::Item: Borrow<()>,
        IR: IntoIterator,
        IR::Item: Borrow<(pso::ShaderStageFlags, Range<u32>)>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn create_pipeline_cache(
        &self,
        _data: Option<&[u8]>,
    ) -> Result<(), device::OutOfMemory> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn get_pipeline_cache_data(&self, _cache: &()) -> Result<Vec<u8>, device::OutOfMemory> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn destroy_pipeline_cache(&self, _: ()) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn create_graphics_pipeline<'a>(
        &self,
        _: &pso::GraphicsPipelineDesc<'a, Backend>,
        _: Option<&()>,
    ) -> Result<(), pso::CreationError> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn create_compute_pipeline<'a>(
        &self,
        _: &pso::ComputePipelineDesc<'a, Backend>,
        _: Option<&()>,
    ) -> Result<(), pso::CreationError> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn merge_pipeline_caches<I>(&self, _: &(), _: I) -> Result<(), device::OutOfMemory>
    where
        I: IntoIterator,
        I::Item: Borrow<()>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn create_framebuffer<I>(
        &self,
        _: &(),
        _: I,
        _: image::Extent,
    ) -> Result<(), device::OutOfMemory>
    where
        I: IntoIterator,
        I::Item: Borrow<()>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn create_shader_module(&self, _: &[u32]) -> Result<(), device::ShaderError> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn create_sampler(&self, _: &image::SamplerDesc) -> Result<(), device::AllocationError> {
        panic!(DO_NOT_USE_MESSAGE)
    }
    unsafe fn create_buffer(&self, _: u64, _: buffer::Usage) -> Result<(), buffer::CreationError> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn get_buffer_requirements(&self, _: &()) -> memory::Requirements {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn bind_buffer_memory(
        &self,
        _: &(),
        _: u64,
        _: &mut (),
    ) -> Result<(), device::BindError> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn create_buffer_view(
        &self,
        _: &(),
        _: Option<format::Format>,
        _: buffer::SubRange,
    ) -> Result<(), buffer::ViewCreationError> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn create_image(
        &self,
        _: image::Kind,
        _: image::Level,
        _: format::Format,
        _: image::Tiling,
        _: image::Usage,
        _: image::ViewCapabilities,
    ) -> Result<(), image::CreationError> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn get_image_requirements(&self, _: &()) -> memory::Requirements {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn get_image_subresource_footprint(
        &self,
        _: &(),
        _: image::Subresource,
    ) -> image::SubresourceFootprint {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn bind_image_memory(
        &self,
        _: &(),
        _: u64,
        _: &mut (),
    ) -> Result<(), device::BindError> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn create_image_view(
        &self,
        _: &(),
        _: image::ViewKind,
        _: format::Format,
        _: format::Swizzle,
        _: image::SubresourceRange,
    ) -> Result<(), image::ViewCreationError> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn create_descriptor_pool<I>(
        &self,
        _: usize,
        _: I,
        _: pso::DescriptorPoolCreateFlags,
    ) -> Result<DescriptorPool, device::OutOfMemory>
    where
        I: IntoIterator,
        I::Item: Borrow<pso::DescriptorRangeDesc>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn create_descriptor_set_layout<I, J>(
        &self,
        _: I,
        _: J,
    ) -> Result<(), device::OutOfMemory>
    where
        I: IntoIterator,
        I::Item: Borrow<pso::DescriptorSetLayoutBinding>,
        J: IntoIterator,
        J::Item: Borrow<()>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn write_descriptor_sets<'a, I, J>(&self, _: I)
    where
        I: IntoIterator<Item = pso::DescriptorSetWrite<'a, Backend, J>>,
        J: IntoIterator,
        J::Item: Borrow<pso::Descriptor<'a, Backend>>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn copy_descriptor_sets<'a, I>(&self, _: I)
    where
        I: IntoIterator,
        I::Item: Borrow<pso::DescriptorSetCopy<'a, Backend>>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    fn create_semaphore(&self) -> Result<(), device::OutOfMemory> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    fn create_fence(&self, _: bool) -> Result<(), device::OutOfMemory> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn get_fence_status(&self, _: &()) -> Result<bool, device::DeviceLost> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    fn create_event(&self) -> Result<(), device::OutOfMemory> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn get_event_status(&self, _: &()) -> Result<bool, device::OomOrDeviceLost> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn set_event(&self, _: &()) -> Result<(), device::OutOfMemory> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn reset_event(&self, _: &()) -> Result<(), device::OutOfMemory> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn create_query_pool(&self, _: query::Type, _: u32) -> Result<(), query::CreationError> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn destroy_query_pool(&self, _: ()) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn get_query_pool_results(
        &self,
        _: &(),
        _: Range<query::Id>,
        _: &mut [u8],
        _: buffer::Offset,
        _: query::ResultFlags,
    ) -> Result<bool, device::OomOrDeviceLost> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn map_memory(&self, _: &(), _: memory::Segment) -> Result<*mut u8, device::MapError> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn unmap_memory(&self, _: &()) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn flush_mapped_memory_ranges<'a, I>(&self, _: I) -> Result<(), device::OutOfMemory>
    where
        I: IntoIterator,
        I::Item: Borrow<(&'a (), memory::Segment)>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn invalidate_mapped_memory_ranges<'a, I>(&self, _: I) -> Result<(), device::OutOfMemory>
    where
        I: IntoIterator,
        I::Item: Borrow<(&'a (), memory::Segment)>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn free_memory(&self, _: ()) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn destroy_shader_module(&self, _: ()) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn destroy_render_pass(&self, _: ()) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn destroy_pipeline_layout(&self, _: ()) {
        panic!(DO_NOT_USE_MESSAGE)
    }
    unsafe fn destroy_graphics_pipeline(&self, _: ()) {
        panic!(DO_NOT_USE_MESSAGE)
    }
    unsafe fn destroy_compute_pipeline(&self, _: ()) {
        panic!(DO_NOT_USE_MESSAGE)
    }
    unsafe fn destroy_framebuffer(&self, _: ()) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn destroy_buffer(&self, _: ()) {
        panic!(DO_NOT_USE_MESSAGE)
    }
    unsafe fn destroy_buffer_view(&self, _: ()) {
        panic!(DO_NOT_USE_MESSAGE)
    }
    unsafe fn destroy_image(&self, _: ()) {
        panic!(DO_NOT_USE_MESSAGE)
    }
    unsafe fn destroy_image_view(&self, _: ()) {
        panic!(DO_NOT_USE_MESSAGE)
    }
    unsafe fn destroy_sampler(&self, _: ()) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn destroy_descriptor_pool(&self, _: DescriptorPool) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn destroy_descriptor_set_layout(&self, _: ()) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn destroy_fence(&self, _: ()) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn destroy_semaphore(&self, _: ()) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn destroy_event(&self, _: ()) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn create_swapchain(
        &self,
        _: &mut Surface,
        _: window::SwapchainConfig,
        _: Option<Swapchain>,
    ) -> Result<(Swapchain, Vec<()>), hal::window::CreationError> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn destroy_swapchain(&self, _: Swapchain) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    fn wait_idle(&self) -> Result<(), device::OutOfMemory> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn set_image_name(&self, _: &mut (), _: &str) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn set_buffer_name(&self, _: &mut (), _: &str) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn set_command_buffer_name(&self, _: &mut CommandBuffer, _: &str) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn set_semaphore_name(&self, _: &mut (), _: &str) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn set_fence_name(&self, _: &mut (), _: &str) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn set_framebuffer_name(&self, _: &mut (), _: &str) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn set_render_pass_name(&self, _: &mut (), _: &str) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn set_descriptor_set_name(&self, _: &mut (), _: &str) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn set_descriptor_set_layout_name(&self, _: &mut (), _: &str) {
        panic!(DO_NOT_USE_MESSAGE)
    }
}

#[derive(Debug)]
pub struct QueueFamily;
impl queue::QueueFamily for QueueFamily {
    fn queue_type(&self) -> queue::QueueType {
        panic!(DO_NOT_USE_MESSAGE)
    }
    fn max_queues(&self) -> usize {
        panic!(DO_NOT_USE_MESSAGE)
    }
    fn id(&self) -> queue::QueueFamilyId {
        panic!(DO_NOT_USE_MESSAGE)
    }
}

/// Dummy raw command pool.
#[derive(Debug)]
pub struct CommandPool;
impl pool::CommandPool<Backend> for CommandPool {
    unsafe fn reset(&mut self, _: bool) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn free<I>(&mut self, _: I)
    where
        I: IntoIterator<Item = CommandBuffer>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }
}

/// Dummy command buffer, which ignores all the calls.
#[derive(Debug)]
pub struct CommandBuffer;
impl command::CommandBuffer<Backend> for CommandBuffer {
    unsafe fn begin(
        &mut self,
        _: command::CommandBufferFlags,
        _: command::CommandBufferInheritanceInfo<Backend>,
    ) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn finish(&mut self) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn reset(&mut self, _: bool) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn pipeline_barrier<'a, T>(
        &mut self,
        _: Range<pso::PipelineStage>,
        _: memory::Dependencies,
        _: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<memory::Barrier<'a, Backend>>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn fill_buffer(&mut self, _: &(), _: buffer::SubRange, _: u32) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn update_buffer(&mut self, _: &(), _: buffer::Offset, _: &[u8]) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn clear_image<T>(&mut self, _: &(), _: image::Layout, _: command::ClearValue, _: T)
    where
        T: IntoIterator,
        T::Item: Borrow<image::SubresourceRange>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn clear_attachments<T, U>(&mut self, _: T, _: U)
    where
        T: IntoIterator,
        T::Item: Borrow<command::AttachmentClear>,
        U: IntoIterator,
        U::Item: Borrow<pso::ClearRect>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn resolve_image<T>(&mut self, _: &(), _: image::Layout, _: &(), _: image::Layout, _: T)
    where
        T: IntoIterator,
        T::Item: Borrow<command::ImageResolve>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn blit_image<T>(
        &mut self,
        _: &(),
        _: image::Layout,
        _: &(),
        _: image::Layout,
        _: image::Filter,
        _: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<command::ImageBlit>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn bind_index_buffer(&mut self, _: buffer::IndexBufferView<Backend>) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn bind_vertex_buffers<I, T>(&mut self, _: u32, _: I)
    where
        I: IntoIterator<Item = (T, buffer::SubRange)>,
        T: Borrow<()>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn set_viewports<T>(&mut self, _: u32, _: T)
    where
        T: IntoIterator,
        T::Item: Borrow<pso::Viewport>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn set_scissors<T>(&mut self, _: u32, _: T)
    where
        T: IntoIterator,
        T::Item: Borrow<pso::Rect>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn set_stencil_reference(&mut self, _: pso::Face, _: pso::StencilValue) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn set_stencil_read_mask(&mut self, _: pso::Face, _: pso::StencilValue) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn set_stencil_write_mask(&mut self, _: pso::Face, _: pso::StencilValue) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn set_blend_constants(&mut self, _: pso::ColorValue) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn set_depth_bounds(&mut self, _: Range<f32>) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn set_line_width(&mut self, _: f32) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn set_depth_bias(&mut self, _: pso::DepthBias) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn begin_render_pass<T>(
        &mut self,
        _: &(),
        _: &(),
        _: pso::Rect,
        _: T,
        _: command::SubpassContents,
    ) where
        T: IntoIterator,
        T::Item: Borrow<command::ClearValue>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn next_subpass(&mut self, _: command::SubpassContents) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn end_render_pass(&mut self) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn bind_graphics_pipeline(&mut self, _: &()) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn bind_graphics_descriptor_sets<I, J>(&mut self, _: &(), _: usize, _: I, _: J)
    where
        I: IntoIterator,
        I::Item: Borrow<()>,
        J: IntoIterator,
        J::Item: Borrow<command::DescriptorSetOffset>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn bind_compute_pipeline(&mut self, _: &()) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn bind_compute_descriptor_sets<I, J>(&mut self, _: &(), _: usize, _: I, _: J)
    where
        I: IntoIterator,
        I::Item: Borrow<()>,
        J: IntoIterator,
        J::Item: Borrow<command::DescriptorSetOffset>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn dispatch(&mut self, _: hal::WorkGroupCount) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn dispatch_indirect(&mut self, _: &(), _: buffer::Offset) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn copy_buffer<T>(&mut self, _: &(), _: &(), _: T)
    where
        T: IntoIterator,
        T::Item: Borrow<command::BufferCopy>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn copy_image<T>(&mut self, _: &(), _: image::Layout, _: &(), _: image::Layout, _: T)
    where
        T: IntoIterator,
        T::Item: Borrow<command::ImageCopy>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn copy_buffer_to_image<T>(&mut self, _: &(), _: &(), _: image::Layout, _: T)
    where
        T: IntoIterator,
        T::Item: Borrow<command::BufferImageCopy>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn copy_image_to_buffer<T>(&mut self, _: &(), _: image::Layout, _: &(), _: T)
    where
        T: IntoIterator,
        T::Item: Borrow<command::BufferImageCopy>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn draw(&mut self, _: Range<hal::VertexCount>, _: Range<hal::InstanceCount>) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn draw_indexed(
        &mut self,
        _: Range<hal::IndexCount>,
        _: hal::VertexOffset,
        _: Range<hal::InstanceCount>,
    ) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn draw_indirect(&mut self, _: &(), _: buffer::Offset, _: hal::DrawCount, _: u32) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn draw_indexed_indirect(
        &mut self,
        _: &(),
        _: buffer::Offset,
        _: hal::DrawCount,
        _: u32,
    ) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn set_event(&mut self, _: &(), _: pso::PipelineStage) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn reset_event(&mut self, _: &(), _: pso::PipelineStage) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn wait_events<'a, I, J>(&mut self, _: I, _: Range<pso::PipelineStage>, _: J)
    where
        I: IntoIterator,
        I::Item: Borrow<()>,
        J: IntoIterator,
        J::Item: Borrow<memory::Barrier<'a, Backend>>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn begin_query(&mut self, _: query::Query<Backend>, _: query::ControlFlags) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn end_query(&mut self, _: query::Query<Backend>) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn reset_query_pool(&mut self, _: &(), _: Range<query::Id>) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn copy_query_pool_results(
        &mut self,
        _: &(),
        _: Range<query::Id>,
        _: &(),
        _: buffer::Offset,
        _: buffer::Offset,
        _: query::ResultFlags,
    ) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn write_timestamp(&mut self, _: pso::PipelineStage, _: query::Query<Backend>) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn push_graphics_constants(
        &mut self,
        _: &(),
        _: pso::ShaderStageFlags,
        _: u32,
        _: &[u32],
    ) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn push_compute_constants(&mut self, _: &(), _: u32, _: &[u32]) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn execute_commands<'a, T, I>(&mut self, _: I)
    where
        T: 'a + Borrow<CommandBuffer>,
        I: IntoIterator<Item = &'a T>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn insert_debug_marker(&mut self, _: &str, _: u32) {
        panic!(DO_NOT_USE_MESSAGE)
    }
    unsafe fn begin_debug_marker(&mut self, _: &str, _: u32) {
        panic!(DO_NOT_USE_MESSAGE)
    }
    unsafe fn end_debug_marker(&mut self) {
        panic!(DO_NOT_USE_MESSAGE)
    }
}

// Dummy descriptor pool.
#[derive(Debug)]
pub struct DescriptorPool;
impl pso::DescriptorPool<Backend> for DescriptorPool {
    unsafe fn free_sets<I>(&mut self, _descriptor_sets: I)
    where
        I: IntoIterator<Item = ()>,
    {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn reset(&mut self) {
        panic!(DO_NOT_USE_MESSAGE)
    }
}

/// Dummy surface.
#[derive(Debug)]
pub struct Surface;
impl window::Surface<Backend> for Surface {
    fn supports_queue_family(&self, _: &QueueFamily) -> bool {
        panic!(DO_NOT_USE_MESSAGE)
    }

    fn capabilities(&self, _: &PhysicalDevice) -> window::SurfaceCapabilities {
        panic!(DO_NOT_USE_MESSAGE)
    }

    fn supported_formats(&self, _: &PhysicalDevice) -> Option<Vec<format::Format>> {
        panic!(DO_NOT_USE_MESSAGE)
    }
}
impl window::PresentationSurface<Backend> for Surface {
    type SwapchainImage = ();

    unsafe fn configure_swapchain(
        &mut self,
        _: &Device,
        _: window::SwapchainConfig,
    ) -> Result<(), window::CreationError> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn unconfigure_swapchain(&mut self, _: &Device) {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn acquire_image(
        &mut self,
        _: u64,
    ) -> Result<((), Option<window::Suboptimal>), window::AcquireError> {
        panic!(DO_NOT_USE_MESSAGE)
    }
}

/// Dummy swapchain.
#[derive(Debug)]
pub struct Swapchain;
impl window::Swapchain<Backend> for Swapchain {
    unsafe fn acquire_image(
        &mut self,
        _: u64,
        _: Option<&()>,
        _: Option<&()>,
    ) -> Result<(window::SwapImageIndex, Option<window::Suboptimal>), window::AcquireError> {
        panic!(DO_NOT_USE_MESSAGE)
    }
}

#[derive(Debug)]
pub struct Instance;

impl hal::Instance<Backend> for Instance {
    fn create(_name: &str, _version: u32) -> Result<Self, hal::UnsupportedBackend> {
        Ok(Instance)
    }

    fn enumerate_adapters(&self) -> Vec<adapter::Adapter<Backend>> {
        vec![]
    }

    unsafe fn create_surface(
        &self,
        _: &impl raw_window_handle::HasRawWindowHandle,
    ) -> Result<Surface, hal::window::InitError> {
        panic!(DO_NOT_USE_MESSAGE)
    }

    unsafe fn destroy_surface(&self, _surface: Surface) {
        panic!(DO_NOT_USE_MESSAGE)
    }
}
