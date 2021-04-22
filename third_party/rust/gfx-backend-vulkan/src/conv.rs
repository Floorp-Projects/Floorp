use crate::native as n;

use ash::vk;

use hal::{
    buffer, command, format, image, memory,
    memory::Segment,
    pass, pso, query,
    window::{CompositeAlphaMode, PresentMode},
    IndexType,
};

use std::mem;

pub fn map_format(format: format::Format) -> vk::Format {
    vk::Format::from_raw(format as i32)
}

pub fn map_vk_format(vk_format: vk::Format) -> Option<format::Format> {
    if (vk_format.as_raw() as usize) < format::NUM_FORMATS && vk_format != vk::Format::UNDEFINED {
        Some(unsafe { mem::transmute(vk_format) })
    } else {
        None
    }
}

pub fn map_tiling(tiling: image::Tiling) -> vk::ImageTiling {
    vk::ImageTiling::from_raw(tiling as i32)
}

pub fn map_component(component: format::Component) -> vk::ComponentSwizzle {
    use hal::format::Component::*;
    match component {
        Zero => vk::ComponentSwizzle::ZERO,
        One => vk::ComponentSwizzle::ONE,
        R => vk::ComponentSwizzle::R,
        G => vk::ComponentSwizzle::G,
        B => vk::ComponentSwizzle::B,
        A => vk::ComponentSwizzle::A,
    }
}

pub fn map_swizzle(swizzle: format::Swizzle) -> vk::ComponentMapping {
    vk::ComponentMapping {
        r: map_component(swizzle.0),
        g: map_component(swizzle.1),
        b: map_component(swizzle.2),
        a: map_component(swizzle.3),
    }
}

pub fn map_index_type(index_type: IndexType) -> vk::IndexType {
    match index_type {
        IndexType::U16 => vk::IndexType::UINT16,
        IndexType::U32 => vk::IndexType::UINT32,
    }
}

pub fn map_image_layout(layout: image::Layout) -> vk::ImageLayout {
    use hal::image::Layout as Il;
    match layout {
        Il::General => vk::ImageLayout::GENERAL,
        Il::ColorAttachmentOptimal => vk::ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
        Il::DepthStencilAttachmentOptimal => vk::ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        Il::DepthStencilReadOnlyOptimal => vk::ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        Il::ShaderReadOnlyOptimal => vk::ImageLayout::SHADER_READ_ONLY_OPTIMAL,
        Il::TransferSrcOptimal => vk::ImageLayout::TRANSFER_SRC_OPTIMAL,
        Il::TransferDstOptimal => vk::ImageLayout::TRANSFER_DST_OPTIMAL,
        Il::Undefined => vk::ImageLayout::UNDEFINED,
        Il::Preinitialized => vk::ImageLayout::PREINITIALIZED,
        Il::Present => vk::ImageLayout::PRESENT_SRC_KHR,
    }
}

pub fn map_image_aspects(aspects: format::Aspects) -> vk::ImageAspectFlags {
    vk::ImageAspectFlags::from_raw(aspects.bits() as u32)
}

pub fn map_offset(offset: image::Offset) -> vk::Offset3D {
    vk::Offset3D {
        x: offset.x,
        y: offset.y,
        z: offset.z,
    }
}

pub fn map_extent(offset: image::Extent) -> vk::Extent3D {
    vk::Extent3D {
        width: offset.width,
        height: offset.height,
        depth: offset.depth,
    }
}

pub fn map_subresource(sub: &image::Subresource) -> vk::ImageSubresource {
    vk::ImageSubresource {
        aspect_mask: map_image_aspects(sub.aspects),
        mip_level: sub.level as _,
        array_layer: sub.layer as _,
    }
}

pub fn map_subresource_layers(sub: &image::SubresourceLayers) -> vk::ImageSubresourceLayers {
    vk::ImageSubresourceLayers {
        aspect_mask: map_image_aspects(sub.aspects),
        mip_level: sub.level as _,
        base_array_layer: sub.layers.start as _,
        layer_count: (sub.layers.end - sub.layers.start) as _,
    }
}

pub fn map_subresource_range(range: &image::SubresourceRange) -> vk::ImageSubresourceRange {
    vk::ImageSubresourceRange {
        aspect_mask: map_image_aspects(range.aspects),
        base_mip_level: range.level_start as _,
        level_count: range
            .level_count
            .map_or(vk::REMAINING_MIP_LEVELS, |c| c as _),
        base_array_layer: range.layer_start as _,
        layer_count: range
            .layer_count
            .map_or(vk::REMAINING_ARRAY_LAYERS, |c| c as _),
    }
}

pub fn map_attachment_load_op(op: pass::AttachmentLoadOp) -> vk::AttachmentLoadOp {
    use hal::pass::AttachmentLoadOp as Alo;
    match op {
        Alo::Load => vk::AttachmentLoadOp::LOAD,
        Alo::Clear => vk::AttachmentLoadOp::CLEAR,
        Alo::DontCare => vk::AttachmentLoadOp::DONT_CARE,
    }
}

pub fn map_attachment_store_op(op: pass::AttachmentStoreOp) -> vk::AttachmentStoreOp {
    use hal::pass::AttachmentStoreOp as Aso;
    match op {
        Aso::Store => vk::AttachmentStoreOp::STORE,
        Aso::DontCare => vk::AttachmentStoreOp::DONT_CARE,
    }
}

pub fn map_buffer_access(access: buffer::Access) -> vk::AccessFlags {
    vk::AccessFlags::from_raw(access.bits())
}

pub fn map_image_access(access: image::Access) -> vk::AccessFlags {
    vk::AccessFlags::from_raw(access.bits())
}

pub fn map_pipeline_stage(stage: pso::PipelineStage) -> vk::PipelineStageFlags {
    vk::PipelineStageFlags::from_raw(stage.bits())
}

pub fn map_buffer_usage(usage: buffer::Usage) -> vk::BufferUsageFlags {
    vk::BufferUsageFlags::from_raw(usage.bits())
}

pub fn map_buffer_create_flags(sparse: memory::SparseFlags) -> vk::BufferCreateFlags {
    vk::BufferCreateFlags::from_raw(sparse.bits())
}

pub fn map_image_usage(usage: image::Usage) -> vk::ImageUsageFlags {
    vk::ImageUsageFlags::from_raw(usage.bits())
}

pub fn map_vk_image_usage(usage: vk::ImageUsageFlags) -> image::Usage {
    image::Usage::from_bits_truncate(usage.as_raw())
}

pub fn map_descriptor_type(ty: pso::DescriptorType) -> vk::DescriptorType {
    match ty {
        pso::DescriptorType::Sampler => vk::DescriptorType::SAMPLER,
        pso::DescriptorType::Image { ty } => match ty {
            pso::ImageDescriptorType::Sampled { with_sampler } => match with_sampler {
                true => vk::DescriptorType::COMBINED_IMAGE_SAMPLER,
                false => vk::DescriptorType::SAMPLED_IMAGE,
            },
            pso::ImageDescriptorType::Storage { .. } => vk::DescriptorType::STORAGE_IMAGE,
        },
        pso::DescriptorType::Buffer { ty, format } => match ty {
            pso::BufferDescriptorType::Storage { .. } => match format {
                pso::BufferDescriptorFormat::Structured { dynamic_offset } => {
                    match dynamic_offset {
                        true => vk::DescriptorType::STORAGE_BUFFER_DYNAMIC,
                        false => vk::DescriptorType::STORAGE_BUFFER,
                    }
                }
                pso::BufferDescriptorFormat::Texel => vk::DescriptorType::STORAGE_TEXEL_BUFFER,
            },
            pso::BufferDescriptorType::Uniform => match format {
                pso::BufferDescriptorFormat::Structured { dynamic_offset } => {
                    match dynamic_offset {
                        true => vk::DescriptorType::UNIFORM_BUFFER_DYNAMIC,
                        false => vk::DescriptorType::UNIFORM_BUFFER,
                    }
                }
                pso::BufferDescriptorFormat::Texel => vk::DescriptorType::UNIFORM_TEXEL_BUFFER,
            },
        },
        pso::DescriptorType::InputAttachment => vk::DescriptorType::INPUT_ATTACHMENT,
    }
}

pub fn map_stage_flags(stages: pso::ShaderStageFlags) -> vk::ShaderStageFlags {
    vk::ShaderStageFlags::from_raw(stages.bits())
}

pub fn map_filter(filter: image::Filter) -> vk::Filter {
    vk::Filter::from_raw(filter as i32)
}

pub fn map_mip_filter(filter: image::Filter) -> vk::SamplerMipmapMode {
    vk::SamplerMipmapMode::from_raw(filter as i32)
}

pub fn map_wrap(wrap: image::WrapMode) -> vk::SamplerAddressMode {
    use hal::image::WrapMode as Wm;
    match wrap {
        Wm::Tile => vk::SamplerAddressMode::REPEAT,
        Wm::Mirror => vk::SamplerAddressMode::MIRRORED_REPEAT,
        Wm::Clamp => vk::SamplerAddressMode::CLAMP_TO_EDGE,
        Wm::Border => vk::SamplerAddressMode::CLAMP_TO_BORDER,
        Wm::MirrorClamp => vk::SamplerAddressMode::MIRROR_CLAMP_TO_EDGE,
    }
}

pub fn map_reduction(reduction: image::ReductionMode) -> vk::SamplerReductionMode {
    use hal::image::ReductionMode as Rm;
    match reduction {
        Rm::WeightedAverage => vk::SamplerReductionMode::WEIGHTED_AVERAGE,
        Rm::Minimum => vk::SamplerReductionMode::MIN,
        Rm::Maximum => vk::SamplerReductionMode::MAX,
    }
}

pub fn map_border_color(border_color: image::BorderColor) -> vk::BorderColor {
    match border_color {
        image::BorderColor::TransparentBlack => vk::BorderColor::FLOAT_TRANSPARENT_BLACK,
        image::BorderColor::OpaqueBlack => vk::BorderColor::FLOAT_OPAQUE_BLACK,
        image::BorderColor::OpaqueWhite => vk::BorderColor::FLOAT_OPAQUE_WHITE,
    }
}

pub fn map_topology(ia: &pso::InputAssemblerDesc) -> vk::PrimitiveTopology {
    match (ia.primitive, ia.with_adjacency) {
        (pso::Primitive::PointList, false) => vk::PrimitiveTopology::POINT_LIST,
        (pso::Primitive::PointList, true) => panic!("Points can't have adjacency info"),
        (pso::Primitive::LineList, false) => vk::PrimitiveTopology::LINE_LIST,
        (pso::Primitive::LineList, true) => vk::PrimitiveTopology::LINE_LIST_WITH_ADJACENCY,
        (pso::Primitive::LineStrip, false) => vk::PrimitiveTopology::LINE_STRIP,
        (pso::Primitive::LineStrip, true) => vk::PrimitiveTopology::LINE_STRIP_WITH_ADJACENCY,
        (pso::Primitive::TriangleList, false) => vk::PrimitiveTopology::TRIANGLE_LIST,
        (pso::Primitive::TriangleList, true) => vk::PrimitiveTopology::TRIANGLE_LIST_WITH_ADJACENCY,
        (pso::Primitive::TriangleStrip, false) => vk::PrimitiveTopology::TRIANGLE_STRIP,
        (pso::Primitive::TriangleStrip, true) => {
            vk::PrimitiveTopology::TRIANGLE_STRIP_WITH_ADJACENCY
        }
        (pso::Primitive::PatchList(_), false) => vk::PrimitiveTopology::PATCH_LIST,
        (pso::Primitive::PatchList(_), true) => panic!("Patches can't have adjacency info"),
    }
}

pub fn map_cull_face(cf: pso::Face) -> vk::CullModeFlags {
    match cf {
        pso::Face::NONE => vk::CullModeFlags::NONE,
        pso::Face::FRONT => vk::CullModeFlags::FRONT,
        pso::Face::BACK => vk::CullModeFlags::BACK,
        _ => vk::CullModeFlags::FRONT_AND_BACK,
    }
}

pub fn map_front_face(ff: pso::FrontFace) -> vk::FrontFace {
    match ff {
        pso::FrontFace::Clockwise => vk::FrontFace::CLOCKWISE,
        pso::FrontFace::CounterClockwise => vk::FrontFace::COUNTER_CLOCKWISE,
    }
}

pub fn map_comparison(fun: pso::Comparison) -> vk::CompareOp {
    use hal::pso::Comparison::*;
    match fun {
        Never => vk::CompareOp::NEVER,
        Less => vk::CompareOp::LESS,
        LessEqual => vk::CompareOp::LESS_OR_EQUAL,
        Equal => vk::CompareOp::EQUAL,
        GreaterEqual => vk::CompareOp::GREATER_OR_EQUAL,
        Greater => vk::CompareOp::GREATER,
        NotEqual => vk::CompareOp::NOT_EQUAL,
        Always => vk::CompareOp::ALWAYS,
    }
}

pub fn map_stencil_op(op: pso::StencilOp) -> vk::StencilOp {
    use hal::pso::StencilOp::*;
    match op {
        Keep => vk::StencilOp::KEEP,
        Zero => vk::StencilOp::ZERO,
        Replace => vk::StencilOp::REPLACE,
        IncrementClamp => vk::StencilOp::INCREMENT_AND_CLAMP,
        IncrementWrap => vk::StencilOp::INCREMENT_AND_WRAP,
        DecrementClamp => vk::StencilOp::DECREMENT_AND_CLAMP,
        DecrementWrap => vk::StencilOp::DECREMENT_AND_WRAP,
        Invert => vk::StencilOp::INVERT,
    }
}

pub fn map_stencil_side(side: &pso::StencilFace) -> vk::StencilOpState {
    vk::StencilOpState {
        fail_op: map_stencil_op(side.op_fail),
        pass_op: map_stencil_op(side.op_pass),
        depth_fail_op: map_stencil_op(side.op_depth_fail),
        compare_op: map_comparison(side.fun),
        compare_mask: !0,
        write_mask: !0,
        reference: 0,
    }
}

pub fn map_blend_factor(factor: pso::Factor) -> vk::BlendFactor {
    use hal::pso::Factor::*;
    match factor {
        Zero => vk::BlendFactor::ZERO,
        One => vk::BlendFactor::ONE,
        SrcColor => vk::BlendFactor::SRC_COLOR,
        OneMinusSrcColor => vk::BlendFactor::ONE_MINUS_SRC_COLOR,
        DstColor => vk::BlendFactor::DST_COLOR,
        OneMinusDstColor => vk::BlendFactor::ONE_MINUS_DST_COLOR,
        SrcAlpha => vk::BlendFactor::SRC_ALPHA,
        OneMinusSrcAlpha => vk::BlendFactor::ONE_MINUS_SRC_ALPHA,
        DstAlpha => vk::BlendFactor::DST_ALPHA,
        OneMinusDstAlpha => vk::BlendFactor::ONE_MINUS_DST_ALPHA,
        ConstColor => vk::BlendFactor::CONSTANT_COLOR,
        OneMinusConstColor => vk::BlendFactor::ONE_MINUS_CONSTANT_COLOR,
        ConstAlpha => vk::BlendFactor::CONSTANT_ALPHA,
        OneMinusConstAlpha => vk::BlendFactor::ONE_MINUS_CONSTANT_ALPHA,
        SrcAlphaSaturate => vk::BlendFactor::SRC_ALPHA_SATURATE,
        Src1Color => vk::BlendFactor::SRC1_COLOR,
        OneMinusSrc1Color => vk::BlendFactor::ONE_MINUS_SRC1_COLOR,
        Src1Alpha => vk::BlendFactor::SRC1_ALPHA,
        OneMinusSrc1Alpha => vk::BlendFactor::ONE_MINUS_SRC1_ALPHA,
    }
}

pub fn map_blend_op(operation: pso::BlendOp) -> (vk::BlendOp, vk::BlendFactor, vk::BlendFactor) {
    use hal::pso::BlendOp::*;
    match operation {
        Add { src, dst } => (
            vk::BlendOp::ADD,
            map_blend_factor(src),
            map_blend_factor(dst),
        ),
        Sub { src, dst } => (
            vk::BlendOp::SUBTRACT,
            map_blend_factor(src),
            map_blend_factor(dst),
        ),
        RevSub { src, dst } => (
            vk::BlendOp::REVERSE_SUBTRACT,
            map_blend_factor(src),
            map_blend_factor(dst),
        ),
        Min => (
            vk::BlendOp::MIN,
            vk::BlendFactor::ZERO,
            vk::BlendFactor::ZERO,
        ),
        Max => (
            vk::BlendOp::MAX,
            vk::BlendFactor::ZERO,
            vk::BlendFactor::ZERO,
        ),
    }
}

pub fn map_pipeline_statistics(
    statistics: query::PipelineStatistic,
) -> vk::QueryPipelineStatisticFlags {
    vk::QueryPipelineStatisticFlags::from_raw(statistics.bits())
}

pub fn map_query_control_flags(flags: query::ControlFlags) -> vk::QueryControlFlags {
    // Safe due to equivalence of HAL values and Vulkan values
    vk::QueryControlFlags::from_raw(flags.bits() & vk::QueryControlFlags::all().as_raw())
}

pub fn map_query_result_flags(flags: query::ResultFlags) -> vk::QueryResultFlags {
    vk::QueryResultFlags::from_raw(flags.bits() & vk::QueryResultFlags::all().as_raw())
}

pub fn map_image_features(
    features: vk::FormatFeatureFlags,
    supports_transfer_bits: bool,
    supports_sampler_filter_minmax: bool,
) -> format::ImageFeature {
    let mut mapped_flags = format::ImageFeature::empty();
    if features.contains(vk::FormatFeatureFlags::SAMPLED_IMAGE) {
        mapped_flags |= format::ImageFeature::SAMPLED;
    }
    if features.contains(vk::FormatFeatureFlags::SAMPLED_IMAGE_FILTER_LINEAR) {
        mapped_flags |= format::ImageFeature::SAMPLED_LINEAR;
    }
    if supports_sampler_filter_minmax
        && features.contains(vk::FormatFeatureFlags::SAMPLED_IMAGE_FILTER_MINMAX)
    {
        mapped_flags |= format::ImageFeature::SAMPLED_MINMAX;
    }

    if features.contains(vk::FormatFeatureFlags::STORAGE_IMAGE) {
        mapped_flags |= format::ImageFeature::STORAGE;
        mapped_flags |= format::ImageFeature::STORAGE_READ_WRITE;
    }
    if features.contains(vk::FormatFeatureFlags::STORAGE_IMAGE_ATOMIC) {
        mapped_flags |= format::ImageFeature::STORAGE_ATOMIC;
    }

    if features.contains(vk::FormatFeatureFlags::COLOR_ATTACHMENT) {
        mapped_flags |= format::ImageFeature::COLOR_ATTACHMENT;
    }
    if features.contains(vk::FormatFeatureFlags::COLOR_ATTACHMENT_BLEND) {
        mapped_flags |= format::ImageFeature::COLOR_ATTACHMENT_BLEND;
    }
    if features.contains(vk::FormatFeatureFlags::DEPTH_STENCIL_ATTACHMENT) {
        mapped_flags |= format::ImageFeature::DEPTH_STENCIL_ATTACHMENT;
    }

    if features.contains(vk::FormatFeatureFlags::BLIT_SRC) {
        mapped_flags |= format::ImageFeature::BLIT_SRC;
        if !supports_transfer_bits {
            mapped_flags |= format::ImageFeature::TRANSFER_SRC;
        }
    }
    if features.contains(vk::FormatFeatureFlags::BLIT_DST) {
        mapped_flags |= format::ImageFeature::BLIT_DST;
        if !supports_transfer_bits {
            mapped_flags |= format::ImageFeature::TRANSFER_DST;
        }
    }
    if supports_transfer_bits {
        if features.contains(vk::FormatFeatureFlags::TRANSFER_SRC) {
            mapped_flags |= format::ImageFeature::TRANSFER_SRC;
        }
        if features.contains(vk::FormatFeatureFlags::TRANSFER_DST) {
            mapped_flags |= format::ImageFeature::TRANSFER_DST;
        }
    }

    mapped_flags
}

pub fn map_buffer_features(features: vk::FormatFeatureFlags) -> format::BufferFeature {
    format::BufferFeature::from_bits_truncate(features.as_raw())
}

pub fn map_memory_range<'a>((memory, segment): (&'a n::Memory, Segment)) -> vk::MappedMemoryRange {
    vk::MappedMemoryRange::builder()
        .memory(memory.raw)
        .offset(segment.offset)
        .size(segment.size.unwrap_or(vk::WHOLE_SIZE))
        .build()
}

pub fn map_command_buffer_flags(flags: command::CommandBufferFlags) -> vk::CommandBufferUsageFlags {
    // Safe due to equivalence of HAL values and Vulkan values
    vk::CommandBufferUsageFlags::from_raw(flags.bits())
}

pub fn map_command_buffer_level(level: command::Level) -> vk::CommandBufferLevel {
    match level {
        command::Level::Primary => vk::CommandBufferLevel::PRIMARY,
        command::Level::Secondary => vk::CommandBufferLevel::SECONDARY,
    }
}

pub fn map_view_kind(
    kind: image::ViewKind,
    ty: vk::ImageType,
    is_cube: bool,
) -> Option<vk::ImageViewType> {
    use crate::image::ViewKind::*;
    use crate::vk::ImageType;

    Some(match (ty, kind) {
        (ImageType::TYPE_1D, D1) => vk::ImageViewType::TYPE_1D,
        (ImageType::TYPE_1D, D1Array) => vk::ImageViewType::TYPE_1D_ARRAY,
        (ImageType::TYPE_2D, D2) => vk::ImageViewType::TYPE_2D,
        (ImageType::TYPE_2D, D2Array) => vk::ImageViewType::TYPE_2D_ARRAY,
        (ImageType::TYPE_3D, D3) => vk::ImageViewType::TYPE_3D,
        (ImageType::TYPE_2D, Cube) if is_cube => vk::ImageViewType::CUBE,
        (ImageType::TYPE_2D, CubeArray) if is_cube => vk::ImageViewType::CUBE_ARRAY,
        (ImageType::TYPE_3D, Cube) if is_cube => vk::ImageViewType::CUBE,
        (ImageType::TYPE_3D, CubeArray) if is_cube => vk::ImageViewType::CUBE_ARRAY,
        _ => return None,
    })
}

pub fn map_rect(rect: &pso::Rect) -> vk::Rect2D {
    vk::Rect2D {
        offset: vk::Offset2D {
            x: rect.x as _,
            y: rect.y as _,
        },
        extent: vk::Extent2D {
            width: rect.w as _,
            height: rect.h as _,
        },
    }
}

pub fn map_clear_rect(rect: &pso::ClearRect) -> vk::ClearRect {
    vk::ClearRect {
        base_array_layer: rect.layers.start as _,
        layer_count: (rect.layers.end - rect.layers.start) as _,
        rect: map_rect(&rect.rect),
    }
}

pub fn map_viewport(vp: &pso::Viewport, flip_y: bool, shift_y: bool) -> vk::Viewport {
    vk::Viewport {
        x: vp.rect.x as _,
        y: if shift_y {
            vp.rect.y + vp.rect.h
        } else {
            vp.rect.y
        } as _,
        width: vp.rect.w as _,
        height: if flip_y { -vp.rect.h } else { vp.rect.h } as _,
        min_depth: vp.depth.start,
        max_depth: vp.depth.end,
    }
}

pub fn map_view_capabilities(caps: image::ViewCapabilities) -> vk::ImageCreateFlags {
    vk::ImageCreateFlags::from_raw(caps.bits())
}

pub fn map_view_capabilities_sparse(
    sparse: memory::SparseFlags,
    caps: image::ViewCapabilities,
) -> vk::ImageCreateFlags {
    vk::ImageCreateFlags::from_raw(sparse.bits() | caps.bits())
}

pub fn map_present_mode(mode: PresentMode) -> vk::PresentModeKHR {
    if mode == PresentMode::IMMEDIATE {
        vk::PresentModeKHR::IMMEDIATE
    } else if mode == PresentMode::MAILBOX {
        vk::PresentModeKHR::MAILBOX
    } else if mode == PresentMode::FIFO {
        vk::PresentModeKHR::FIFO
    } else if mode == PresentMode::RELAXED {
        vk::PresentModeKHR::FIFO_RELAXED
    } else {
        panic!("Unexpected present mode {:?}", mode)
    }
}

pub fn map_vk_present_mode(mode: vk::PresentModeKHR) -> PresentMode {
    if mode == vk::PresentModeKHR::IMMEDIATE {
        PresentMode::IMMEDIATE
    } else if mode == vk::PresentModeKHR::MAILBOX {
        PresentMode::MAILBOX
    } else if mode == vk::PresentModeKHR::FIFO {
        PresentMode::FIFO
    } else if mode == vk::PresentModeKHR::FIFO_RELAXED {
        PresentMode::RELAXED
    } else {
        warn!("Unrecognized present mode {:?}", mode);
        PresentMode::IMMEDIATE
    }
}

pub fn map_composite_alpha_mode(
    composite_alpha_mode: CompositeAlphaMode,
) -> vk::CompositeAlphaFlagsKHR {
    vk::CompositeAlphaFlagsKHR::from_raw(composite_alpha_mode.bits())
}

pub fn map_vk_composite_alpha(composite_alpha: vk::CompositeAlphaFlagsKHR) -> CompositeAlphaMode {
    CompositeAlphaMode::from_bits_truncate(composite_alpha.as_raw())
}

pub fn map_descriptor_pool_create_flags(
    flags: pso::DescriptorPoolCreateFlags,
) -> vk::DescriptorPoolCreateFlags {
    vk::DescriptorPoolCreateFlags::from_raw(flags.bits())
}

pub fn map_sample_count_flags(samples: image::NumSamples) -> vk::SampleCountFlags {
    vk::SampleCountFlags::from_raw((samples as u32) & vk::SampleCountFlags::all().as_raw())
}

pub fn map_vk_memory_properties(flags: vk::MemoryPropertyFlags) -> hal::memory::Properties {
    use crate::memory::Properties;
    let mut properties = Properties::empty();

    if flags.contains(vk::MemoryPropertyFlags::DEVICE_LOCAL) {
        properties |= Properties::DEVICE_LOCAL;
    }
    if flags.contains(vk::MemoryPropertyFlags::HOST_VISIBLE) {
        properties |= Properties::CPU_VISIBLE;
    }
    if flags.contains(vk::MemoryPropertyFlags::HOST_COHERENT) {
        properties |= Properties::COHERENT;
    }
    if flags.contains(vk::MemoryPropertyFlags::HOST_CACHED) {
        properties |= Properties::CPU_CACHED;
    }
    if flags.contains(vk::MemoryPropertyFlags::LAZILY_ALLOCATED) {
        properties |= Properties::LAZILY_ALLOCATED;
    }

    properties
}

pub fn map_vk_memory_heap_flags(flags: vk::MemoryHeapFlags) -> hal::memory::HeapFlags {
    use hal::memory::HeapFlags;
    let mut hal_flags = HeapFlags::empty();

    if flags.contains(vk::MemoryHeapFlags::DEVICE_LOCAL) {
        hal_flags |= HeapFlags::DEVICE_LOCAL;
    }

    hal_flags
}
