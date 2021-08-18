use ash::{
    extensions::{khr::DrawIndirectCount, khr::Swapchain, nv::MeshShader},
    version::{DeviceV1_0, InstanceV1_0},
    vk,
};

#[cfg(unix)]
use ash::extensions::khr::ExternalMemoryFd;

use hal::{
    adapter,
    device::{CreationError, OutOfMemory},
    display, external_memory, format, image,
    pso::PatchSize,
    queue, DescriptorLimits, DownlevelProperties, DynamicStates, ExternalMemoryLimits, Features,
    Limits, PhysicalDeviceProperties,
};

use std::{ffi::CStr, fmt, mem, ptr, sync::Arc};

use crate::{
    conv, info, native, Backend, Device, DeviceExtensionFunctions, ExtensionFn, Queue, QueueFamily,
    RawDevice, RawInstance, Version,
};

/// Aggregate of the `vk::PhysicalDevice*Features` structs used by `gfx`.
#[derive(Debug, Default)]
pub struct PhysicalDeviceFeatures {
    core: vk::PhysicalDeviceFeatures,
    vulkan_1_2: Option<vk::PhysicalDeviceVulkan12Features>,
    descriptor_indexing: Option<vk::PhysicalDeviceDescriptorIndexingFeaturesEXT>,
    mesh_shader: Option<vk::PhysicalDeviceMeshShaderFeaturesNV>,
    imageless_framebuffer: Option<vk::PhysicalDeviceImagelessFramebufferFeaturesKHR>,
}

// This is safe because the structs have `p_next: *mut c_void`, which we null out/never read.
unsafe impl Send for PhysicalDeviceFeatures {}
unsafe impl Sync for PhysicalDeviceFeatures {}

impl PhysicalDeviceFeatures {
    /// Add the members of `self` into `info.enabled_features` and its `p_next` chain.
    fn add_to_device_create_builder<'a>(
        &'a mut self,
        mut info: vk::DeviceCreateInfoBuilder<'a>,
    ) -> vk::DeviceCreateInfoBuilder<'a> {
        info = info.enabled_features(&self.core);

        if let Some(ref mut feature) = self.vulkan_1_2 {
            info = info.push_next(feature);
        }
        if let Some(ref mut feature) = self.descriptor_indexing {
            info = info.push_next(feature);
        }
        if let Some(ref mut feature) = self.mesh_shader {
            info = info.push_next(feature);
        }
        if let Some(ref mut feature) = self.imageless_framebuffer {
            info = info.push_next(feature);
        }

        info
    }

    /// Create a `PhysicalDeviceFeatures` that will be used to create a logical device.
    ///
    /// `requested_features` should be the same as what was used to generate `enabled_extensions`.
    fn from_extensions_and_requested_features(
        api_version: Version,
        enabled_extensions: &[&'static CStr],
        requested_features: Features,
        supports_vulkan12_imageless_framebuffer: bool,
    ) -> PhysicalDeviceFeatures {
        // This must follow the "Valid Usage" requirements of [`VkDeviceCreateInfo`](https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkDeviceCreateInfo.html).
        let features = requested_features;
        PhysicalDeviceFeatures {
            // vk::PhysicalDeviceFeatures is a struct composed of Bool32's while
            // Features is a bitfield so we need to map everything manually
            core: vk::PhysicalDeviceFeatures::builder()
                .robust_buffer_access(features.contains(Features::ROBUST_BUFFER_ACCESS))
                .full_draw_index_uint32(features.contains(Features::FULL_DRAW_INDEX_U32))
                .image_cube_array(features.contains(Features::IMAGE_CUBE_ARRAY))
                .independent_blend(features.contains(Features::INDEPENDENT_BLENDING))
                .geometry_shader(features.contains(Features::GEOMETRY_SHADER))
                .tessellation_shader(features.contains(Features::TESSELLATION_SHADER))
                .sample_rate_shading(features.contains(Features::SAMPLE_RATE_SHADING))
                .dual_src_blend(features.contains(Features::DUAL_SRC_BLENDING))
                .logic_op(features.contains(Features::LOGIC_OP))
                .multi_draw_indirect(features.contains(Features::MULTI_DRAW_INDIRECT))
                .draw_indirect_first_instance(
                    features.contains(Features::DRAW_INDIRECT_FIRST_INSTANCE),
                )
                .depth_clamp(features.contains(Features::DEPTH_CLAMP))
                .depth_bias_clamp(features.contains(Features::DEPTH_BIAS_CLAMP))
                .fill_mode_non_solid(features.contains(Features::NON_FILL_POLYGON_MODE))
                .depth_bounds(features.contains(Features::DEPTH_BOUNDS))
                .wide_lines(features.contains(Features::LINE_WIDTH))
                .large_points(features.contains(Features::POINT_SIZE))
                .alpha_to_one(features.contains(Features::ALPHA_TO_ONE))
                .multi_viewport(features.contains(Features::MULTI_VIEWPORTS))
                .sampler_anisotropy(features.contains(Features::SAMPLER_ANISOTROPY))
                .texture_compression_etc2(features.contains(Features::FORMAT_ETC2))
                .texture_compression_astc_ldr(features.contains(Features::FORMAT_ASTC_LDR))
                .texture_compression_bc(features.contains(Features::FORMAT_BC))
                .occlusion_query_precise(features.contains(Features::PRECISE_OCCLUSION_QUERY))
                .pipeline_statistics_query(features.contains(Features::PIPELINE_STATISTICS_QUERY))
                .vertex_pipeline_stores_and_atomics(
                    features.contains(Features::VERTEX_STORES_AND_ATOMICS),
                )
                .fragment_stores_and_atomics(
                    features.contains(Features::FRAGMENT_STORES_AND_ATOMICS),
                )
                .shader_tessellation_and_geometry_point_size(
                    features.contains(Features::SHADER_TESSELLATION_AND_GEOMETRY_POINT_SIZE),
                )
                .shader_image_gather_extended(
                    features.contains(Features::SHADER_IMAGE_GATHER_EXTENDED),
                )
                .shader_storage_image_extended_formats(
                    features.contains(Features::SHADER_STORAGE_IMAGE_EXTENDED_FORMATS),
                )
                .shader_storage_image_multisample(
                    features.contains(Features::SHADER_STORAGE_IMAGE_MULTISAMPLE),
                )
                .shader_storage_image_read_without_format(
                    features.contains(Features::SHADER_STORAGE_IMAGE_READ_WITHOUT_FORMAT),
                )
                .shader_storage_image_write_without_format(
                    features.contains(Features::SHADER_STORAGE_IMAGE_WRITE_WITHOUT_FORMAT),
                )
                .shader_uniform_buffer_array_dynamic_indexing(
                    features.contains(Features::SHADER_UNIFORM_BUFFER_ARRAY_DYNAMIC_INDEXING),
                )
                .shader_sampled_image_array_dynamic_indexing(
                    features.contains(Features::SHADER_SAMPLED_IMAGE_ARRAY_DYNAMIC_INDEXING),
                )
                .shader_storage_buffer_array_dynamic_indexing(
                    features.contains(Features::SHADER_STORAGE_BUFFER_ARRAY_DYNAMIC_INDEXING),
                )
                .shader_storage_image_array_dynamic_indexing(
                    features.contains(Features::SHADER_STORAGE_IMAGE_ARRAY_DYNAMIC_INDEXING),
                )
                .shader_clip_distance(features.contains(Features::SHADER_CLIP_DISTANCE))
                .shader_cull_distance(features.contains(Features::SHADER_CULL_DISTANCE))
                .shader_float64(features.contains(Features::SHADER_FLOAT64))
                .shader_int64(features.contains(Features::SHADER_INT64))
                .shader_int16(features.contains(Features::SHADER_INT16))
                .shader_resource_residency(features.contains(Features::SHADER_RESOURCE_RESIDENCY))
                .shader_resource_min_lod(features.contains(Features::SHADER_RESOURCE_MIN_LOD))
                .sparse_binding(features.contains(Features::SPARSE_BINDING))
                .sparse_residency_buffer(features.contains(Features::SPARSE_RESIDENCY_BUFFER))
                .sparse_residency_image2_d(features.contains(Features::SPARSE_RESIDENCY_IMAGE_2D))
                .sparse_residency_image3_d(features.contains(Features::SPARSE_RESIDENCY_IMAGE_3D))
                .sparse_residency2_samples(features.contains(Features::SPARSE_RESIDENCY_2_SAMPLES))
                .sparse_residency4_samples(features.contains(Features::SPARSE_RESIDENCY_4_SAMPLES))
                .sparse_residency8_samples(features.contains(Features::SPARSE_RESIDENCY_8_SAMPLES))
                .sparse_residency16_samples(
                    features.contains(Features::SPARSE_RESIDENCY_16_SAMPLES),
                )
                .sparse_residency_aliased(features.contains(Features::SPARSE_RESIDENCY_ALIASED))
                .variable_multisample_rate(features.contains(Features::VARIABLE_MULTISAMPLE_RATE))
                .inherited_queries(features.contains(Features::INHERITED_QUERIES))
                .build(),
            vulkan_1_2: if api_version >= Version::V1_2 {
                Some(
                    vk::PhysicalDeviceVulkan12Features::builder()
                        .sampler_mirror_clamp_to_edge(
                            features.contains(Features::SAMPLER_MIRROR_CLAMP_EDGE),
                        )
                        .draw_indirect_count(features.contains(Features::DRAW_INDIRECT_COUNT))
                        .descriptor_indexing(
                            features.intersects(Features::DESCRIPTOR_INDEXING_MASK),
                        )
                        .shader_sampled_image_array_non_uniform_indexing(
                            features.contains(Features::SAMPLED_TEXTURE_DESCRIPTOR_INDEXING),
                        )
                        .shader_storage_image_array_non_uniform_indexing(
                            features.contains(Features::STORAGE_TEXTURE_DESCRIPTOR_INDEXING),
                        )
                        .shader_storage_buffer_array_non_uniform_indexing(
                            features.contains(Features::STORAGE_BUFFER_DESCRIPTOR_INDEXING),
                        )
                        .shader_uniform_buffer_array_non_uniform_indexing(
                            features.contains(Features::UNIFORM_BUFFER_DESCRIPTOR_INDEXING),
                        )
                        .runtime_descriptor_array(
                            features.contains(Features::UNSIZED_DESCRIPTOR_ARRAY),
                        )
                        .sampler_filter_minmax(features.contains(Features::SAMPLER_REDUCTION))
                        .imageless_framebuffer(supports_vulkan12_imageless_framebuffer)
                        .build(),
                )
            } else {
                None
            },
            descriptor_indexing: if enabled_extensions
                .contains(&vk::ExtDescriptorIndexingFn::name())
            {
                Some(
                    vk::PhysicalDeviceDescriptorIndexingFeaturesEXT::builder()
                        .shader_sampled_image_array_non_uniform_indexing(
                            features.contains(Features::SAMPLED_TEXTURE_DESCRIPTOR_INDEXING),
                        )
                        .shader_storage_image_array_non_uniform_indexing(
                            features.contains(Features::STORAGE_TEXTURE_DESCRIPTOR_INDEXING),
                        )
                        .shader_storage_buffer_array_non_uniform_indexing(
                            features.contains(Features::STORAGE_BUFFER_DESCRIPTOR_INDEXING),
                        )
                        .shader_uniform_buffer_array_non_uniform_indexing(
                            features.contains(Features::UNIFORM_BUFFER_DESCRIPTOR_INDEXING),
                        )
                        .runtime_descriptor_array(
                            features.contains(Features::UNSIZED_DESCRIPTOR_ARRAY),
                        )
                        .build(),
                )
            } else {
                None
            },
            mesh_shader: if enabled_extensions.contains(&vk::NvMeshShaderFn::name()) {
                Some(
                    vk::PhysicalDeviceMeshShaderFeaturesNV::builder()
                        .task_shader(features.contains(Features::TASK_SHADER))
                        .mesh_shader(features.contains(Features::MESH_SHADER))
                        .build(),
                )
            } else {
                None
            },
            imageless_framebuffer: if enabled_extensions
                .contains(&vk::KhrImagelessFramebufferFn::name())
            {
                Some(
                    vk::PhysicalDeviceImagelessFramebufferFeaturesKHR::builder()
                        .imageless_framebuffer(true)
                        .build(),
                )
            } else {
                None
            },
        }
    }

    /// Get the `hal::Features` corresponding to the raw physical device's features and properties.
    fn to_hal_features(&self, info: &PhysicalDeviceInfo) -> Features {
        let mut bits = Features::empty()
            | Features::TRIANGLE_FAN
            | Features::SEPARATE_STENCIL_REF_VALUES
            | Features::SAMPLER_MIP_LOD_BIAS
            | Features::SAMPLER_BORDER_COLOR
            | Features::MUTABLE_COMPARISON_SAMPLER
            | Features::MUTABLE_UNNORMALIZED_SAMPLER
            | Features::TEXTURE_DESCRIPTOR_ARRAY
            | Features::BUFFER_DESCRIPTOR_ARRAY;

        if self.core.robust_buffer_access != 0 {
            bits |= Features::ROBUST_BUFFER_ACCESS;
        }
        if self.core.full_draw_index_uint32 != 0 {
            bits |= Features::FULL_DRAW_INDEX_U32;
        }
        if self.core.image_cube_array != 0 {
            bits |= Features::IMAGE_CUBE_ARRAY;
        }
        if self.core.independent_blend != 0 {
            bits |= Features::INDEPENDENT_BLENDING;
        }
        if self.core.geometry_shader != 0 {
            bits |= Features::GEOMETRY_SHADER;
        }
        if self.core.tessellation_shader != 0 {
            bits |= Features::TESSELLATION_SHADER;
        }
        if self.core.sample_rate_shading != 0 {
            bits |= Features::SAMPLE_RATE_SHADING;
        }
        if self.core.dual_src_blend != 0 {
            bits |= Features::DUAL_SRC_BLENDING;
        }
        if self.core.logic_op != 0 {
            bits |= Features::LOGIC_OP;
        }
        if self.core.multi_draw_indirect != 0 {
            bits |= Features::MULTI_DRAW_INDIRECT;
        }
        if self.core.draw_indirect_first_instance != 0 {
            bits |= Features::DRAW_INDIRECT_FIRST_INSTANCE;
        }
        if self.core.depth_clamp != 0 {
            bits |= Features::DEPTH_CLAMP;
        }
        if self.core.depth_bias_clamp != 0 {
            bits |= Features::DEPTH_BIAS_CLAMP;
        }
        if self.core.fill_mode_non_solid != 0 {
            bits |= Features::NON_FILL_POLYGON_MODE;
        }
        if self.core.depth_bounds != 0 {
            bits |= Features::DEPTH_BOUNDS;
        }
        if self.core.wide_lines != 0 {
            bits |= Features::LINE_WIDTH;
        }
        if self.core.large_points != 0 {
            bits |= Features::POINT_SIZE;
        }
        if self.core.alpha_to_one != 0 {
            bits |= Features::ALPHA_TO_ONE;
        }
        if self.core.multi_viewport != 0 {
            bits |= Features::MULTI_VIEWPORTS;
        }
        if self.core.sampler_anisotropy != 0 {
            bits |= Features::SAMPLER_ANISOTROPY;
        }
        if self.core.texture_compression_etc2 != 0 {
            bits |= Features::FORMAT_ETC2;
        }
        if self.core.texture_compression_astc_ldr != 0 {
            bits |= Features::FORMAT_ASTC_LDR;
        }
        if self.core.texture_compression_bc != 0 {
            bits |= Features::FORMAT_BC;
        }
        if self.core.occlusion_query_precise != 0 {
            bits |= Features::PRECISE_OCCLUSION_QUERY;
        }
        if self.core.pipeline_statistics_query != 0 {
            bits |= Features::PIPELINE_STATISTICS_QUERY;
        }
        if self.core.vertex_pipeline_stores_and_atomics != 0 {
            bits |= Features::VERTEX_STORES_AND_ATOMICS;
        }
        if self.core.fragment_stores_and_atomics != 0 {
            bits |= Features::FRAGMENT_STORES_AND_ATOMICS;
        }
        if self.core.shader_tessellation_and_geometry_point_size != 0 {
            bits |= Features::SHADER_TESSELLATION_AND_GEOMETRY_POINT_SIZE;
        }
        if self.core.shader_image_gather_extended != 0 {
            bits |= Features::SHADER_IMAGE_GATHER_EXTENDED;
        }
        if self.core.shader_storage_image_extended_formats != 0 {
            bits |= Features::SHADER_STORAGE_IMAGE_EXTENDED_FORMATS;
        }
        if self.core.shader_storage_image_multisample != 0 {
            bits |= Features::SHADER_STORAGE_IMAGE_MULTISAMPLE;
        }
        if self.core.shader_storage_image_read_without_format != 0 {
            bits |= Features::SHADER_STORAGE_IMAGE_READ_WITHOUT_FORMAT;
        }
        if self.core.shader_storage_image_write_without_format != 0 {
            bits |= Features::SHADER_STORAGE_IMAGE_WRITE_WITHOUT_FORMAT;
        }
        if self.core.shader_uniform_buffer_array_dynamic_indexing != 0 {
            bits |= Features::SHADER_UNIFORM_BUFFER_ARRAY_DYNAMIC_INDEXING;
        }
        if self.core.shader_sampled_image_array_dynamic_indexing != 0 {
            bits |= Features::SHADER_SAMPLED_IMAGE_ARRAY_DYNAMIC_INDEXING;
        }
        if self.core.shader_storage_buffer_array_dynamic_indexing != 0 {
            bits |= Features::SHADER_STORAGE_BUFFER_ARRAY_DYNAMIC_INDEXING;
        }
        if self.core.shader_storage_image_array_dynamic_indexing != 0 {
            bits |= Features::SHADER_STORAGE_IMAGE_ARRAY_DYNAMIC_INDEXING;
        }
        if self.core.shader_clip_distance != 0 {
            bits |= Features::SHADER_CLIP_DISTANCE;
        }
        if self.core.shader_cull_distance != 0 {
            bits |= Features::SHADER_CULL_DISTANCE;
        }
        if self.core.shader_float64 != 0 {
            bits |= Features::SHADER_FLOAT64;
        }
        if self.core.shader_int64 != 0 {
            bits |= Features::SHADER_INT64;
        }
        if self.core.shader_int16 != 0 {
            bits |= Features::SHADER_INT16;
        }
        if self.core.shader_resource_residency != 0 {
            bits |= Features::SHADER_RESOURCE_RESIDENCY;
        }
        if self.core.shader_resource_min_lod != 0 {
            bits |= Features::SHADER_RESOURCE_MIN_LOD;
        }
        if self.core.sparse_binding != 0 {
            bits |= Features::SPARSE_BINDING;
        }
        if self.core.sparse_residency_buffer != 0 {
            bits |= Features::SPARSE_RESIDENCY_BUFFER;
        }
        if self.core.sparse_residency_image2_d != 0 {
            bits |= Features::SPARSE_RESIDENCY_IMAGE_2D;
        }
        if self.core.sparse_residency_image3_d != 0 {
            bits |= Features::SPARSE_RESIDENCY_IMAGE_3D;
        }
        if self.core.sparse_residency2_samples != 0 {
            bits |= Features::SPARSE_RESIDENCY_2_SAMPLES;
        }
        if self.core.sparse_residency4_samples != 0 {
            bits |= Features::SPARSE_RESIDENCY_4_SAMPLES;
        }
        if self.core.sparse_residency8_samples != 0 {
            bits |= Features::SPARSE_RESIDENCY_8_SAMPLES;
        }
        if self.core.sparse_residency16_samples != 0 {
            bits |= Features::SPARSE_RESIDENCY_16_SAMPLES;
        }
        if self.core.sparse_residency_aliased != 0 {
            bits |= Features::SPARSE_RESIDENCY_ALIASED;
        }
        if self.core.variable_multisample_rate != 0 {
            bits |= Features::VARIABLE_MULTISAMPLE_RATE;
        }
        if self.core.inherited_queries != 0 {
            bits |= Features::INHERITED_QUERIES;
        }

        if info.supports_extension(vk::AmdNegativeViewportHeightFn::name())
            || info.supports_extension(vk::KhrMaintenance1Fn::name())
            || info.api_version() >= Version::V1_1
        {
            bits |= Features::NDC_Y_UP;
        }

        if info.supports_extension(vk::KhrSamplerMirrorClampToEdgeFn::name()) {
            bits |= Features::SAMPLER_MIRROR_CLAMP_EDGE;
        }

        if info.supports_extension(vk::ExtSamplerFilterMinmaxFn::name()) {
            bits |= Features::SAMPLER_REDUCTION;
        }

        if info.supports_extension(DrawIndirectCount::name()) {
            bits |= Features::DRAW_INDIRECT_COUNT
        }

        if info.supports_extension(vk::ExtConservativeRasterizationFn::name()) {
            bits |= Features::CONSERVATIVE_RASTERIZATION
        }

        if info.api_version() >= Version::V1_1
            || (info.supports_extension(vk::KhrGetPhysicalDeviceProperties2Fn::name())
                && info.supports_extension(vk::KhrExternalMemoryFn::name()))
        {
            bits |= Features::EXTERNAL_MEMORY
        }

        if let Some(ref vulkan_1_2) = self.vulkan_1_2 {
            if vulkan_1_2.shader_sampled_image_array_non_uniform_indexing != 0 {
                bits |= Features::SAMPLED_TEXTURE_DESCRIPTOR_INDEXING;
            }
            if vulkan_1_2.shader_storage_image_array_non_uniform_indexing != 0 {
                bits |= Features::STORAGE_TEXTURE_DESCRIPTOR_INDEXING;
            }
            if vulkan_1_2.shader_storage_buffer_array_non_uniform_indexing != 0 {
                bits |= Features::STORAGE_BUFFER_DESCRIPTOR_INDEXING;
            }
            if vulkan_1_2.shader_uniform_buffer_array_non_uniform_indexing != 0 {
                bits |= Features::UNIFORM_BUFFER_DESCRIPTOR_INDEXING;
            }
            if vulkan_1_2.runtime_descriptor_array != 0 {
                bits |= Features::UNSIZED_DESCRIPTOR_ARRAY;
            }
            if vulkan_1_2.sampler_mirror_clamp_to_edge != 0 {
                bits |= Features::SAMPLER_MIRROR_CLAMP_EDGE;
            }
            if vulkan_1_2.sampler_filter_minmax != 0 {
                bits |= Features::SAMPLER_REDUCTION
            }
            if vulkan_1_2.draw_indirect_count != 0 {
                bits |= Features::DRAW_INDIRECT_COUNT
            }
        }

        if let Some(ref descriptor_indexing) = self.descriptor_indexing {
            if descriptor_indexing.shader_sampled_image_array_non_uniform_indexing != 0 {
                bits |= Features::SAMPLED_TEXTURE_DESCRIPTOR_INDEXING;
            }
            if descriptor_indexing.shader_storage_image_array_non_uniform_indexing != 0 {
                bits |= Features::STORAGE_TEXTURE_DESCRIPTOR_INDEXING;
            }
            if descriptor_indexing.shader_storage_buffer_array_non_uniform_indexing != 0 {
                bits |= Features::STORAGE_BUFFER_DESCRIPTOR_INDEXING;
            }
            if descriptor_indexing.shader_uniform_buffer_array_non_uniform_indexing != 0 {
                bits |= Features::UNIFORM_BUFFER_DESCRIPTOR_INDEXING;
            }
            if descriptor_indexing.runtime_descriptor_array != 0 {
                bits |= Features::UNSIZED_DESCRIPTOR_ARRAY;
            }
        }

        if let Some(ref mesh_shader) = self.mesh_shader {
            if mesh_shader.task_shader != 0 {
                bits |= Features::TASK_SHADER;
            }
            if mesh_shader.mesh_shader != 0 {
                bits |= Features::MESH_SHADER;
            }
        }

        bits
    }
}

/// Information gathered about a physical device. Used to
pub struct PhysicalDeviceInfo {
    supported_extensions: Vec<vk::ExtensionProperties>,
    properties: vk::PhysicalDeviceProperties,
}

impl PhysicalDeviceInfo {
    fn api_version(&self) -> Version {
        self.properties.api_version.into()
    }

    fn supports_extension(&self, extension: &CStr) -> bool {
        self.supported_extensions
            .iter()
            .any(|ep| unsafe { CStr::from_ptr(ep.extension_name.as_ptr()) } == extension)
    }

    /// Map `requested_features` to the list of Vulkan extension strings required to create the logical device.
    fn get_required_extensions(&self, requested_features: Features) -> Vec<&'static CStr> {
        let mut requested_extensions = Vec::new();

        requested_extensions.push(Swapchain::name());

        if self.api_version() < Version::V1_1 {
            requested_extensions.push(vk::KhrMaintenance1Fn::name());
            requested_extensions.push(vk::KhrMaintenance2Fn::name());
        }

        if requested_features.contains(Features::NDC_Y_UP) {
            // `VK_AMD_negative_viewport_height` is obsoleted by `VK_KHR_maintenance1` and must not be enabled alongside `VK_KHR_maintenance1` or a 1.1+ device.
            if self.api_version() < Version::V1_1
                && !self.supports_extension(vk::KhrMaintenance1Fn::name())
            {
                requested_extensions.push(vk::AmdNegativeViewportHeightFn::name());
            }
        }

        if self.api_version() < Version::V1_2
            && self.supports_extension(vk::KhrImagelessFramebufferFn::name())
        {
            requested_extensions.push(vk::KhrImagelessFramebufferFn::name());
            requested_extensions.push(vk::KhrImageFormatListFn::name()); // Required for `KhrImagelessFramebufferFn`
        }

        if self.api_version() < Version::V1_2 {
            requested_extensions.push(vk::ExtSamplerFilterMinmaxFn::name());
        }

        if self.api_version() < Version::V1_2
            && requested_features.intersects(Features::DESCRIPTOR_INDEXING_MASK)
        {
            requested_extensions.push(vk::ExtDescriptorIndexingFn::name());

            if self.api_version() < Version::V1_1 {
                requested_extensions.push(vk::KhrMaintenance3Fn::name());
            }
        }

        if self.api_version() < Version::V1_2
            && requested_features.intersects(Features::SAMPLER_MIRROR_CLAMP_EDGE)
        {
            requested_extensions.push(vk::KhrSamplerMirrorClampToEdgeFn::name());
        }

        if self.api_version() < Version::V1_2
            && requested_features.contains(Features::SAMPLER_REDUCTION)
        {
            requested_extensions.push(vk::ExtSamplerFilterMinmaxFn::name());
        }

        if requested_features.intersects(Features::MESH_SHADER_MASK) {
            requested_extensions.push(MeshShader::name());
        }

        if self.api_version() < Version::V1_2
            && requested_features.contains(Features::DRAW_INDIRECT_COUNT)
        {
            requested_extensions.push(DrawIndirectCount::name());
        }

        if requested_features.contains(Features::CONSERVATIVE_RASTERIZATION) {
            requested_extensions.push(vk::ExtConservativeRasterizationFn::name());
            requested_extensions.push(vk::KhrGetDisplayProperties2Fn::name()); // TODO NOT NEEDED, RIGHT?
        }

        if self.supports_extension(vk::ExtDisplayControlFn::name()) {
            requested_extensions.push(vk::ExtDisplayControlFn::name());
        }

        if requested_features.contains(Features::EXTERNAL_MEMORY) {
            if self.api_version() < Version::V1_1 {
                requested_extensions.push(vk::KhrGetPhysicalDeviceProperties2Fn::name());
                requested_extensions.push(vk::KhrExternalMemoryFn::name());

                // External memory interact with DedicatedAllocation extension, but it is not a strict dependency.
                requested_extensions.push(vk::KhrGetMemoryRequirements2Fn::name()); // TODO Functions should be added because they are useful
                requested_extensions.push(vk::KhrDedicatedAllocationFn::name());
            }

            requested_extensions.push(vk::ExtExternalMemoryHostFn::name());
            #[cfg(window)]
            requested_extensions.push(vk::KhrExternalMemoryWin32Fn::name());
            #[cfg(unix)]
            {
                requested_extensions.push(vk::KhrExternalMemoryFdFn::name());
                requested_extensions.push(vk::ExtExternalMemoryDmaBufFn::name());

                requested_extensions.push(vk::KhrBindMemory2Fn::name());

                requested_extensions.push(vk::KhrImageFormatListFn::name());
                requested_extensions.push(vk::KhrSamplerYcbcrConversionFn::name());
                requested_extensions.push(vk::ExtImageDrmFormatModifierFn::name());
            }
        }
        requested_extensions
    }

    fn load(
        instance: &Arc<RawInstance>,
        device: vk::PhysicalDevice,
    ) -> (Self, PhysicalDeviceFeatures) {
        let device_properties = unsafe {
            PhysicalDeviceInfo {
                supported_extensions: instance
                    .inner
                    .enumerate_device_extension_properties(device)
                    .unwrap(),
                properties: instance.inner.get_physical_device_properties(device),
            }
        };

        let mut features = PhysicalDeviceFeatures::default();
        features.core = if let Some(ref get_device_properties) =
            instance.get_physical_device_properties
        {
            let core = vk::PhysicalDeviceFeatures::builder().build();
            let mut features2 = vk::PhysicalDeviceFeatures2KHR::builder()
                .features(core)
                .build();

            if device_properties.api_version() >= Version::V1_2 {
                features.vulkan_1_2 = Some(vk::PhysicalDeviceVulkan12Features::builder().build());

                let mut_ref = features.vulkan_1_2.as_mut().unwrap();
                mut_ref.p_next = mem::replace(&mut features2.p_next, mut_ref as *mut _ as *mut _);
            }

            if device_properties.supports_extension(vk::ExtDescriptorIndexingFn::name()) {
                features.descriptor_indexing =
                    Some(vk::PhysicalDeviceDescriptorIndexingFeaturesEXT::builder().build());

                let mut_ref = features.descriptor_indexing.as_mut().unwrap();
                mut_ref.p_next = mem::replace(&mut features2.p_next, mut_ref as *mut _ as *mut _);
            }

            if device_properties.supports_extension(MeshShader::name()) {
                features.mesh_shader =
                    Some(vk::PhysicalDeviceMeshShaderFeaturesNV::builder().build());

                let mut_ref = features.mesh_shader.as_mut().unwrap();
                mut_ref.p_next = mem::replace(&mut features2.p_next, mut_ref as *mut _ as *mut _);
            }

            // `VK_KHR_imageless_framebuffer` is promoted to 1.2, but has no changes, so we can keep using the extension unconditionally.
            if device_properties.supports_extension(vk::KhrImagelessFramebufferFn::name()) {
                features.imageless_framebuffer =
                    Some(vk::PhysicalDeviceImagelessFramebufferFeaturesKHR::builder().build());

                let mut_ref = features.imageless_framebuffer.as_mut().unwrap();
                mut_ref.p_next = mem::replace(&mut features2.p_next, mut_ref as *mut _ as *mut _);
            }

            match get_device_properties {
                ExtensionFn::Promoted => {
                    use ash::version::InstanceV1_1;
                    unsafe {
                        instance
                            .inner
                            .get_physical_device_features2(device, &mut features2);
                    }
                }
                ExtensionFn::Extension(get_device_properties) => unsafe {
                    get_device_properties
                        .get_physical_device_features2_khr(device, &mut features2 as *mut _);
                },
            }

            features2.features
        } else {
            unsafe { instance.inner.get_physical_device_features(device) }
        };

        /// # Safety
        /// `T` must be a struct bigger than `vk::BaseOutStructure`.
        unsafe fn null_p_next<T>(features: &mut Option<T>) {
            if let Some(features) = features {
                // This is technically invalid since `vk::BaseOutStructure` and `T` will probably never have the same size.
                mem::transmute::<_, &mut vk::BaseOutStructure>(features).p_next = ptr::null_mut();
            }
        }

        unsafe {
            null_p_next(&mut features.vulkan_1_2);
            null_p_next(&mut features.descriptor_indexing);
            null_p_next(&mut features.mesh_shader);
            null_p_next(&mut features.imageless_framebuffer);
        }

        (device_properties, features)
    }
}

pub struct PhysicalDevice {
    instance: Arc<RawInstance>,
    pub handle: vk::PhysicalDevice,
    known_memory_flags: vk::MemoryPropertyFlags,
    device_info: PhysicalDeviceInfo,
    device_features: PhysicalDeviceFeatures,
    available_features: Features,
}

impl PhysicalDevice {
    /// # Safety
    /// `raw_device` must be created from `self` (or from the inner raw handle)
    /// `raw_device` must be created with `requested_features`
    pub unsafe fn gpu_from_raw(
        &self,
        raw_device: ash::Device,
        families: &[(&QueueFamily, &[queue::QueuePriority])],
        requested_features: Features,
    ) -> Result<adapter::Gpu<Backend>, CreationError> {
        let enabled_extensions = self.enabled_extensions(requested_features)?;
        Ok(self.inner_create_gpu(
            raw_device,
            true,
            families,
            requested_features,
            enabled_extensions,
        ))
    }

    unsafe fn inner_create_gpu(
        &self,
        device_raw: ash::Device,
        handle_is_external: bool,
        families: &[(&QueueFamily, &[queue::QueuePriority])],
        requested_features: Features,
        enabled_extensions: Vec<&CStr>,
    ) -> adapter::Gpu<Backend> {
        let valid_ash_memory_types = {
            let mem_properties = self
                .instance
                .inner
                .get_physical_device_memory_properties(self.handle);
            mem_properties.memory_types[..mem_properties.memory_type_count as usize]
                .iter()
                .enumerate()
                .fold(0, |u, (i, mem)| {
                    if self.known_memory_flags.contains(mem.property_flags) {
                        u | (1 << i)
                    } else {
                        u
                    }
                })
        };

        let supports_vulkan12_imageless_framebuffer = self
            .device_features
            .vulkan_1_2
            .map_or(false, |features| features.imageless_framebuffer == vk::TRUE);

        let swapchain_fn = Swapchain::new(&self.instance.inner, &device_raw);

        let mesh_fn = if enabled_extensions.contains(&MeshShader::name()) {
            Some(ExtensionFn::Extension(MeshShader::new(
                &self.instance.inner,
                &device_raw,
            )))
        } else {
            None
        };

        let indirect_count_fn = if enabled_extensions.contains(&DrawIndirectCount::name()) {
            Some(ExtensionFn::Extension(DrawIndirectCount::new(
                &self.instance.inner,
                &device_raw,
            )))
        } else if self.device_info.api_version() >= Version::V1_2 {
            Some(ExtensionFn::Promoted)
        } else {
            None
        };

        let display_control = if enabled_extensions.contains(&vk::ExtDisplayControlFn::name()) {
            Some(vk::ExtDisplayControlFn::load(|name| {
                std::mem::transmute(
                    self.instance
                        .inner
                        .get_device_proc_addr(device_raw.handle(), name.as_ptr()),
                )
            }))
        } else {
            None
        };

        let memory_requirements2 =
            if enabled_extensions.contains(&vk::KhrGetMemoryRequirements2Fn::name()) {
                Some(ExtensionFn::Extension(
                    vk::KhrGetMemoryRequirements2Fn::load(|name| {
                        std::mem::transmute(
                            self.instance
                                .inner
                                .get_device_proc_addr(device_raw.handle(), name.as_ptr()),
                        )
                    }),
                ))
            } else {
                None
            };

        let dedicated_allocation;
        let external_memory;
        let external_memory_host;

        #[cfg(unix)]
        let external_memory_fd;

        #[cfg(any(target_os = "linux", target_os = "android"))]
        let external_memory_dma_buf;

        #[cfg(any(target_os = "linux", target_os = "android"))]
        let image_drm_format_modifier;

        #[cfg(windows)]
        let external_memory_win32;

        if requested_features.contains(Features::EXTERNAL_MEMORY) {
            if self.device_info.api_version() < Version::V1_1 {
                external_memory = if enabled_extensions.contains(&vk::KhrExternalMemoryFn::name()) {
                    Some(ExtensionFn::Extension(()))
                } else {
                    None
                };

                // External memory interact with DedicatedAllocation extension, but it is not a strict dependency.
                dedicated_allocation =
                    if enabled_extensions.contains(&vk::KhrDedicatedAllocationFn::name()) {
                        Some(ExtensionFn::Extension(()))
                    } else {
                        None
                    };
            } else {
                external_memory = Some(ExtensionFn::Promoted);
                dedicated_allocation = Some(ExtensionFn::Promoted);
            }

            external_memory_host =
                if enabled_extensions.contains(&vk::ExtExternalMemoryHostFn::name()) {
                    Some(vk::ExtExternalMemoryHostFn::load(|name| {
                        std::mem::transmute(
                            self.instance
                                .inner
                                .get_device_proc_addr(device_raw.handle(), name.as_ptr()),
                        )
                    }))
                } else {
                    None
                };

            #[cfg(windows)]
            {
                external_memory_win32 =
                    if enabled_extensions.contains(&vk::KhrExternalMemoryWin32Fn::name()) {
                        Some(vk::KhrExternalMemoryWin32Fn::load(|name| {
                            std::mem::transmute(
                                self.instance
                                    .inner
                                    .get_device_proc_addr(device_raw.handle(), name.as_ptr()),
                            )
                        }))
                    } else {
                        None
                    };
            }
            #[cfg(unix)]
            {
                external_memory_fd = if enabled_extensions.contains(&ExternalMemoryFd::name()) {
                    Some(ExternalMemoryFd::new(&self.instance.inner, &device_raw))
                } else {
                    None
                };

                #[cfg(any(target_os = "linux", target_os = "android"))]
                {
                    external_memory_dma_buf =
                        if enabled_extensions.contains(&vk::ExtExternalMemoryDmaBufFn::name()) {
                            Some(())
                        } else {
                            None
                        };

                    image_drm_format_modifier =
                        if enabled_extensions.contains(&vk::ExtImageDrmFormatModifierFn::name()) {
                            Some(vk::ExtImageDrmFormatModifierFn::load(|name| {
                                std::mem::transmute(
                                    self.instance
                                        .inner
                                        .get_device_proc_addr(device_raw.handle(), name.as_ptr()),
                                )
                            }))
                        } else {
                            None
                        };
                }
            }
        } else {
            dedicated_allocation = None;
            external_memory = None;
            external_memory_host = None;

            #[cfg(unix)]
            {
                external_memory_fd = None;
            }

            #[cfg(any(target_os = "linux", target_os = "android"))]
            {
                external_memory_dma_buf = None;
                image_drm_format_modifier = None;
            }

            #[cfg(windows)]
            {
                external_memory_win32 = None;
            }
        }

        #[cfg(feature = "naga")]
        let naga_options = {
            use naga::back::spv;
            let capabilities = [
                spv::Capability::Shader,
                spv::Capability::Matrix,
                spv::Capability::InputAttachment,
                spv::Capability::Sampled1D,
                spv::Capability::Image1D,
                spv::Capability::SampledBuffer,
                spv::Capability::ImageBuffer,
                spv::Capability::ImageQuery,
                spv::Capability::DerivativeControl,
                //TODO: fill out the rest
            ];
            let mut flags = spv::WriterFlags::empty();
            flags.set(spv::WriterFlags::DEBUG, cfg!(debug_assertions));
            flags.set(
                spv::WriterFlags::ADJUST_COORDINATE_SPACE,
                !requested_features.contains(hal::Features::NDC_Y_UP),
            );
            spv::Options {
                lang_version: (1, 0),
                flags,
                capabilities: Some(capabilities.iter().cloned().collect()),
            }
        };

        let device = Device {
            shared: Arc::new(RawDevice {
                raw: device_raw,
                handle_is_external,
                features: requested_features,
                instance: Arc::clone(&self.instance),
                extension_fns: DeviceExtensionFunctions {
                    mesh_shaders: mesh_fn,
                    draw_indirect_count: indirect_count_fn,
                    display_control,
                    memory_requirements2: memory_requirements2,
                    dedicated_allocation: dedicated_allocation,
                    external_memory,
                    external_memory_host,
                    #[cfg(unix)]
                    external_memory_fd,
                    #[cfg(windows)]
                    external_memory_win32,
                    #[cfg(any(target_os = "linux", target_os = "android"))]
                    external_memory_dma_buf,
                    #[cfg(any(target_os = "linux", target_os = "android"))]
                    image_drm_format_modifier,
                },
                flip_y_requires_shift: self.device_info.api_version() >= Version::V1_1
                    || self
                        .device_info
                        .supports_extension(vk::KhrMaintenance1Fn::name()),
                imageless_framebuffers: supports_vulkan12_imageless_framebuffer
                    || self
                        .device_info
                        .supports_extension(vk::KhrImagelessFramebufferFn::name()),
                image_view_usage: self.device_info.api_version() >= Version::V1_1
                    || self
                        .device_info
                        .supports_extension(vk::KhrMaintenance2Fn::name()),
                timestamp_period: self.device_info.properties.limits.timestamp_period,
            }),
            vendor_id: self.device_info.properties.vendor_id,
            valid_ash_memory_types,
            render_doc: Default::default(),
            #[cfg(feature = "naga")]
            naga_options,
        };

        let device_arc = Arc::clone(&device.shared);
        let queue_groups = families
            .iter()
            .map(|&(family, ref priorities)| {
                let mut family_raw =
                    queue::QueueGroup::new(queue::QueueFamilyId(family.index as usize));
                for id in 0..priorities.len() {
                    let queue_raw = device_arc.raw.get_device_queue(family.index, id as _);
                    family_raw.add_queue(Queue {
                        raw: Arc::new(queue_raw),
                        device: device_arc.clone(),
                        swapchain_fn: swapchain_fn.clone(),
                    });
                }
                family_raw
            })
            .collect();

        adapter::Gpu {
            device,
            queue_groups,
        }
    }

    pub fn enabled_extensions(
        &self,
        requested_features: Features,
    ) -> Result<Vec<&'static CStr>, CreationError> {
        use adapter::PhysicalDevice;

        if !self.features().contains(requested_features) {
            return Err(CreationError::MissingFeature);
        }

        let (supported_extensions, unsupported_extensions) = self
            .device_info
            .get_required_extensions(requested_features)
            .iter()
            .partition::<Vec<&CStr>, _>(|&&extension| {
                self.device_info.supports_extension(extension)
            });

        if !unsupported_extensions.is_empty() {
            warn!("Missing extensions: {:?}", unsupported_extensions);
        }

        debug!("Supported extensions: {:?}", supported_extensions);

        Ok(supported_extensions)
    }
}

pub(crate) fn load_adapter(
    instance: &Arc<RawInstance>,
    device: vk::PhysicalDevice,
) -> adapter::Adapter<Backend> {
    let (device_info, device_features) = PhysicalDeviceInfo::load(instance, device);

    let info = adapter::AdapterInfo {
        name: unsafe {
            CStr::from_ptr(device_info.properties.device_name.as_ptr())
                .to_str()
                .unwrap_or("Unknown")
                .to_owned()
        },
        vendor: device_info.properties.vendor_id as usize,
        device: device_info.properties.device_id as usize,
        device_type: match device_info.properties.device_type {
            ash::vk::PhysicalDeviceType::OTHER => adapter::DeviceType::Other,
            ash::vk::PhysicalDeviceType::INTEGRATED_GPU => adapter::DeviceType::IntegratedGpu,
            ash::vk::PhysicalDeviceType::DISCRETE_GPU => adapter::DeviceType::DiscreteGpu,
            ash::vk::PhysicalDeviceType::VIRTUAL_GPU => adapter::DeviceType::VirtualGpu,
            ash::vk::PhysicalDeviceType::CPU => adapter::DeviceType::Cpu,
            _ => adapter::DeviceType::Other,
        },
    };

    let available_features = {
        let mut bits = device_features.to_hal_features(&device_info);

        // see https://github.com/gfx-rs/gfx/issues/1930
        let is_windows_intel_dual_src_bug = cfg!(windows)
            && device_info.properties.vendor_id == info::intel::VENDOR
            && (device_info.properties.device_id & info::intel::DEVICE_KABY_LAKE_MASK
                == info::intel::DEVICE_KABY_LAKE_MASK
                || device_info.properties.device_id & info::intel::DEVICE_SKY_LAKE_MASK
                    == info::intel::DEVICE_SKY_LAKE_MASK);
        if is_windows_intel_dual_src_bug {
            bits.set(Features::DUAL_SRC_BLENDING, false);
        }

        bits
    };

    let physical_device = PhysicalDevice {
        instance: instance.clone(),
        handle: device,
        known_memory_flags: vk::MemoryPropertyFlags::DEVICE_LOCAL
            | vk::MemoryPropertyFlags::HOST_VISIBLE
            | vk::MemoryPropertyFlags::HOST_COHERENT
            | vk::MemoryPropertyFlags::HOST_CACHED
            | vk::MemoryPropertyFlags::LAZILY_ALLOCATED,
        device_info,
        device_features,
        available_features,
    };

    let queue_families = unsafe {
        instance
            .inner
            .get_physical_device_queue_family_properties(device)
            .into_iter()
            .enumerate()
            .map(|(i, properties)| QueueFamily {
                properties,
                device,
                index: i as u32,
            })
            .collect()
    };

    adapter::Adapter {
        info,
        physical_device,
        queue_families,
    }
}

impl fmt::Debug for PhysicalDevice {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("PhysicalDevice").finish()
    }
}

impl adapter::PhysicalDevice<Backend> for PhysicalDevice {
    unsafe fn open(
        &self,
        families: &[(&QueueFamily, &[queue::QueuePriority])],
        requested_features: Features,
    ) -> Result<adapter::Gpu<Backend>, CreationError> {
        let family_infos = families
            .iter()
            .map(|&(family, priorities)| {
                vk::DeviceQueueCreateInfo::builder()
                    .flags(vk::DeviceQueueCreateFlags::empty())
                    .queue_family_index(family.index)
                    .queue_priorities(priorities)
                    .build()
            })
            .collect::<Vec<_>>();

        let enabled_extensions = self.enabled_extensions(requested_features)?;

        let supports_vulkan12_imageless_framebuffer = self
            .device_features
            .vulkan_1_2
            .map_or(false, |features| features.imageless_framebuffer == vk::TRUE);

        // Create device
        let device_raw = {
            let str_pointers = enabled_extensions
                .iter()
                .map(|&s| {
                    // Safe because `enabled_extensions` entries have static lifetime.
                    s.as_ptr()
                })
                .collect::<Vec<_>>();

            let mut enabled_features =
                PhysicalDeviceFeatures::from_extensions_and_requested_features(
                    self.device_info.api_version(),
                    &enabled_extensions,
                    requested_features,
                    supports_vulkan12_imageless_framebuffer,
                );
            let info = vk::DeviceCreateInfo::builder()
                .queue_create_infos(&family_infos)
                .enabled_extension_names(&str_pointers);
            let info = enabled_features.add_to_device_create_builder(info);

            match self.instance.inner.create_device(self.handle, &info, None) {
                Ok(device) => device,
                Err(e) => {
                    return Err(match e {
                        vk::Result::ERROR_OUT_OF_HOST_MEMORY => {
                            CreationError::OutOfMemory(OutOfMemory::Host)
                        }
                        vk::Result::ERROR_OUT_OF_DEVICE_MEMORY => {
                            CreationError::OutOfMemory(OutOfMemory::Device)
                        }
                        vk::Result::ERROR_INITIALIZATION_FAILED => {
                            CreationError::InitializationFailed
                        }
                        vk::Result::ERROR_DEVICE_LOST => CreationError::DeviceLost,
                        vk::Result::ERROR_TOO_MANY_OBJECTS => CreationError::TooManyObjects,
                        _ => {
                            error!("Unknown device creation error: {:?}", e);
                            CreationError::InitializationFailed
                        }
                    })
                }
            }
        };

        Ok(self.inner_create_gpu(
            device_raw,
            false,
            families,
            requested_features,
            enabled_extensions,
        ))
    }

    fn format_properties(&self, format: Option<format::Format>) -> format::Properties {
        let supports_transfer_bits = self
            .device_info
            .supports_extension(vk::KhrMaintenance1Fn::name());

        let supports_sampler_filter_minmax = self
            .available_features
            .contains(Features::SAMPLER_REDUCTION);

        let (properties, drm_format_properties) = unsafe {
            match self.instance.get_physical_device_properties {
                None => {
                    let format_properties =
                        self.instance.inner.get_physical_device_format_properties(
                            self.handle,
                            format.map_or(vk::Format::UNDEFINED, conv::map_format),
                        );
                    (format_properties, Vec::new())
                }
                Some(ref extension) => {
                    let mut raw_format_modifiers: Vec<vk::DrmFormatModifierPropertiesEXT> = Vec::new();
                    let mut drm_format_properties =
                        vk::DrmFormatModifierPropertiesListEXT::builder().build();
                    let mut format_properties2 = vk::FormatProperties2::builder()
                        .push_next(&mut drm_format_properties)
                        .build();
                    // Ash does not implement the "double call" behaviour for this function, so it is implemented here.
                    match extension {
                        ExtensionFn::Promoted => {
                            use ash::version::InstanceV1_1;
                            self.instance.inner.get_physical_device_format_properties2(
                                self.handle,
                                format.map_or(vk::Format::UNDEFINED, conv::map_format),
                                &mut format_properties2,
                            );
                            raw_format_modifiers.reserve_exact(drm_format_properties.drm_format_modifier_count as usize);
                            drm_format_properties.p_drm_format_modifier_properties = raw_format_modifiers.as_mut_ptr();
                            self.instance.inner.get_physical_device_format_properties2(
                                self.handle,
                                format.map_or(vk::Format::UNDEFINED, conv::map_format),
                                &mut format_properties2,
                            );
                            raw_format_modifiers.set_len(drm_format_properties.drm_format_modifier_count as usize);
                        }
                        ExtensionFn::Extension(extension) => {
                            extension.get_physical_device_format_properties2_khr(
                                self.handle,
                                format.map_or(vk::Format::UNDEFINED, conv::map_format),
                                &mut format_properties2,
                            );
                            raw_format_modifiers.reserve_exact(drm_format_properties.drm_format_modifier_count as usize);
                            drm_format_properties.p_drm_format_modifier_properties = raw_format_modifiers.as_mut_ptr();
                            extension.get_physical_device_format_properties2_khr(
                                self.handle,
                                format.map_or(vk::Format::UNDEFINED, conv::map_format),
                                &mut format_properties2,
                            );
                            raw_format_modifiers.set_len(drm_format_properties.drm_format_modifier_count as usize);
                        }
                    }

                    let format_modifiers: Vec<format::DrmFormatProperties> = raw_format_modifiers
                        .into_iter()
                        .filter_map(|format_modifier_properties| {
                            let format_modifier = format::DrmModifier::from(
                                format_modifier_properties.drm_format_modifier,
                            );
                            if let format::DrmModifier::Unrecognized(value) = format_modifier {
                                error!("Unrecognized drm format modifier: {:#?}", value);
                                None
                            } else {
                                Some(format::DrmFormatProperties {
                                    drm_modifier: format_modifier,
                                    plane_count: format_modifier_properties
                                        .drm_format_modifier_plane_count,
                                    valid_usages: conv::map_image_features(
                                        format_modifier_properties
                                            .drm_format_modifier_tiling_features,
                                        supports_transfer_bits,
                                        supports_sampler_filter_minmax,
                                    ),
                                })
                            }
                        })
                        .collect();
                    (format_properties2.format_properties, format_modifiers)
                }
            }
        };

        format::Properties {
            linear_tiling: conv::map_image_features(
                properties.linear_tiling_features,
                supports_transfer_bits,
                supports_sampler_filter_minmax,
            ),
            optimal_tiling: conv::map_image_features(
                properties.optimal_tiling_features,
                supports_transfer_bits,
                supports_sampler_filter_minmax,
            ),
            buffer_features: conv::map_buffer_features(properties.buffer_features),
            drm_format_properties,
        }
    }

    fn image_format_properties(
        &self,
        format: format::Format,
        dimensions: u8,
        tiling: image::Tiling,
        usage: image::Usage,
        view_caps: image::ViewCapabilities,
    ) -> Option<image::FormatProperties> {
        let format_properties = unsafe {
            self.instance
                .inner
                .get_physical_device_image_format_properties(
                    self.handle,
                    conv::map_format(format),
                    match dimensions {
                        1 => vk::ImageType::TYPE_1D,
                        2 => vk::ImageType::TYPE_2D,
                        3 => vk::ImageType::TYPE_3D,
                        _ => panic!("Unexpected image dimensionality: {}", dimensions),
                    },
                    conv::map_tiling(tiling),
                    conv::map_image_usage(usage),
                    conv::map_view_capabilities(view_caps),
                )
        };

        match format_properties {
            Ok(props) => Some(image::FormatProperties {
                max_extent: image::Extent {
                    width: props.max_extent.width,
                    height: props.max_extent.height,
                    depth: props.max_extent.depth,
                },
                max_levels: props.max_mip_levels as _,
                max_layers: props.max_array_layers as _,
                sample_count_mask: props.sample_counts.as_raw() as _,
                max_resource_size: props.max_resource_size as _,
            }),
            Err(vk::Result::ERROR_FORMAT_NOT_SUPPORTED) => None,
            Err(other) => {
                error!("Unexpected error in `image_format_properties`: {:?}", other);
                None
            }
        }
    }

    fn memory_properties(&self) -> adapter::MemoryProperties {
        let mem_properties = unsafe {
            self.instance
                .inner
                .get_physical_device_memory_properties(self.handle)
        };
        let memory_heaps = mem_properties.memory_heaps[..mem_properties.memory_heap_count as usize]
            .iter()
            .map(|mem| adapter::MemoryHeap {
                size: mem.size,
                flags: conv::map_vk_memory_heap_flags(mem.flags),
            })
            .collect();
        let memory_types = mem_properties.memory_types[..mem_properties.memory_type_count as usize]
            .iter()
            .filter_map(|mem| {
                if self.known_memory_flags.contains(mem.property_flags) {
                    Some(adapter::MemoryType {
                        properties: conv::map_vk_memory_properties(mem.property_flags),
                        heap_index: mem.heap_index as usize,
                    })
                } else {
                    warn!(
                        "Skipping memory type with unknown flags {:?}",
                        mem.property_flags
                    );
                    None
                }
            })
            .collect();

        adapter::MemoryProperties {
            memory_heaps,
            memory_types,
        }
    }

    fn external_buffer_properties(
        &self,
        usage: hal::buffer::Usage,
        sparse: hal::memory::SparseFlags,
        external_memory_type: external_memory::ExternalMemoryType,
    ) -> external_memory::ExternalMemoryProperties {
        let external_memory_type_flags: hal::external_memory::ExternalMemoryTypeFlags =
            external_memory_type.into();
        let vk_external_memory_type =
            vk::ExternalMemoryHandleTypeFlags::from_raw(external_memory_type_flags.bits());

        let external_buffer_info = vk::PhysicalDeviceExternalBufferInfo::builder()
            .flags(conv::map_buffer_create_flags(sparse))
            .usage(conv::map_buffer_usage(usage))
            .handle_type(vk_external_memory_type)
            .build();

        let vk_mem_properties = match self.instance.external_memory_capabilities.as_ref() {
            Some(ExtensionFn::Extension(external_memory_capabilities_extension)) => {
                let mut external_buffer_properties =
                    vk::ExternalBufferProperties::builder().build();
                unsafe {
                    external_memory_capabilities_extension
                        .get_physical_device_external_buffer_properties_khr(
                            self.handle,
                            &external_buffer_info,
                            &mut external_buffer_properties,
                        )
                };
                external_buffer_properties.external_memory_properties
            }
            Some(ExtensionFn::Promoted) => {
                use ash::version::InstanceV1_1;
                let mut external_buffer_properties =
                    vk::ExternalBufferProperties::builder().build();
                unsafe {
                    self.instance
                        .inner
                        .get_physical_device_external_buffer_properties(
                            self.handle,
                            &external_buffer_info,
                            &mut external_buffer_properties,
                        )
                }
                external_buffer_properties.external_memory_properties
            }
            None => panic!(
                "This function rely on `Feature::EXTERNAL_MEMORY`, but the feature is not enabled"
            ),
        };

        let mut external_memory_properties = external_memory::ExternalMemoryProperties::empty();
        if vk_mem_properties
            .external_memory_features
            .contains(vk::ExternalMemoryFeatureFlags::EXPORTABLE)
        {
            external_memory_properties |= external_memory::ExternalMemoryProperties::EXPORTABLE;
        }

        if vk_mem_properties
            .external_memory_features
            .contains(vk::ExternalMemoryFeatureFlags::IMPORTABLE)
        {
            external_memory_properties |= external_memory::ExternalMemoryProperties::IMPORTABLE;
        }

        if vk_mem_properties
            .export_from_imported_handle_types
            .contains(vk_external_memory_type)
        {
            external_memory_properties |=
                external_memory::ExternalMemoryProperties::EXPORTABLE_FROM_IMPORTED;
        }

        external_memory_properties
    }

    fn external_image_properties(
        &self,
        format: format::Format,
        dimensions: u8,
        tiling: image::Tiling,
        usage: image::Usage,
        view_caps: image::ViewCapabilities,
        external_memory_type: external_memory::ExternalMemoryType,
    ) -> Result<external_memory::ExternalMemoryProperties, external_memory::ExternalImagePropertiesError>
    {
        if self.instance.external_memory_capabilities.is_none() {
            panic!(
                "This function rely on `Feature::EXTERNAL_MEMORY`, but the feature is not enabled"
            );
        }

        use ash::version::InstanceV1_1;
        let external_memory_type_flags: hal::external_memory::ExternalMemoryTypeFlags =
            external_memory_type.into();
        let vk_external_memory_type =
            vk::ExternalMemoryHandleTypeFlags::from_raw(external_memory_type_flags.bits());

        let mut external_image_format_info = vk::PhysicalDeviceExternalImageFormatInfo::builder()
            .handle_type(vk_external_memory_type)
            .build();
        let image_format_info = vk::PhysicalDeviceImageFormatInfo2::builder()
            .push_next(&mut external_image_format_info)
            .format(conv::map_format(format))
            .ty(match dimensions {
                1 => vk::ImageType::TYPE_1D,
                2 => vk::ImageType::TYPE_2D,
                3 => vk::ImageType::TYPE_3D,
                _ => panic!("Unexpected image dimensionality: {}", dimensions),
            })
            .tiling(conv::map_tiling(tiling))
            .usage(conv::map_image_usage(usage))
            .flags(conv::map_view_capabilities(view_caps))
            .build();

        let mut external_image_format_properties =
            vk::ExternalImageFormatProperties::builder().build();
        let mut image_format_properties = vk::ImageFormatProperties2::builder()
            .push_next(&mut external_image_format_properties)
            .build();

        match unsafe {
            self.instance
                .inner
                .get_physical_device_image_format_properties2(
                    self.handle,
                    &image_format_info,
                    &mut image_format_properties,
                )
        } {
            Ok(_) => {
                let vk_mem_properties = external_image_format_properties.external_memory_properties;

                let mut external_memory_properties =
                    external_memory::ExternalMemoryProperties::empty();
                if vk_mem_properties
                    .external_memory_features
                    .contains(vk::ExternalMemoryFeatureFlags::EXPORTABLE)
                {
                    external_memory_properties |=
                        external_memory::ExternalMemoryProperties::EXPORTABLE;
                }

                if vk_mem_properties
                    .external_memory_features
                    .contains(vk::ExternalMemoryFeatureFlags::IMPORTABLE)
                {
                    external_memory_properties |=
                        external_memory::ExternalMemoryProperties::IMPORTABLE;
                }

                if vk_mem_properties
                    .export_from_imported_handle_types
                    .contains(vk_external_memory_type)
                {
                    external_memory_properties |=
                        external_memory::ExternalMemoryProperties::EXPORTABLE_FROM_IMPORTED;
                }
                Ok(external_memory_properties)
            }
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(OutOfMemory::Host.into()),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(OutOfMemory::Device.into()),
            Err(vk::Result::ERROR_FORMAT_NOT_SUPPORTED) => {
                Err(external_memory::ExternalImagePropertiesError::FormatNotSupported)
            }
            Err(err) => {
                panic!("Unexpected error: {:#?}", err);
            }
        }
    }

    fn features(&self) -> Features {
        self.available_features
    }

    fn properties(&self) -> PhysicalDeviceProperties {
        let limits = {
            let limits = &self.device_info.properties.limits;

            let max_group_count = limits.max_compute_work_group_count;
            let max_group_size = limits.max_compute_work_group_size;

            Limits {
                max_image_1d_size: limits.max_image_dimension1_d,
                max_image_2d_size: limits.max_image_dimension2_d,
                max_image_3d_size: limits.max_image_dimension3_d,
                max_image_cube_size: limits.max_image_dimension_cube,
                max_image_array_layers: limits.max_image_array_layers as _,
                max_texel_elements: limits.max_texel_buffer_elements as _,
                max_patch_size: limits.max_tessellation_patch_size as PatchSize,
                max_viewports: limits.max_viewports as _,
                max_viewport_dimensions: limits.max_viewport_dimensions,
                max_framebuffer_extent: image::Extent {
                    width: limits.max_framebuffer_width,
                    height: limits.max_framebuffer_height,
                    depth: limits.max_framebuffer_layers,
                },
                max_compute_work_group_count: [
                    max_group_count[0] as _,
                    max_group_count[1] as _,
                    max_group_count[2] as _,
                ],
                max_compute_work_group_size: [
                    max_group_size[0] as _,
                    max_group_size[1] as _,
                    max_group_size[2] as _,
                ],
                max_vertex_input_attributes: limits.max_vertex_input_attributes as _,
                max_vertex_input_bindings: limits.max_vertex_input_bindings as _,
                max_vertex_input_attribute_offset: limits.max_vertex_input_attribute_offset as _,
                max_vertex_input_binding_stride: limits.max_vertex_input_binding_stride as _,
                max_vertex_output_components: limits.max_vertex_output_components as _,
                optimal_buffer_copy_offset_alignment: limits.optimal_buffer_copy_offset_alignment
                    as _,
                optimal_buffer_copy_pitch_alignment: limits.optimal_buffer_copy_row_pitch_alignment
                    as _,
                min_texel_buffer_offset_alignment: limits.min_texel_buffer_offset_alignment as _,
                min_uniform_buffer_offset_alignment: limits.min_uniform_buffer_offset_alignment
                    as _,
                min_storage_buffer_offset_alignment: limits.min_storage_buffer_offset_alignment
                    as _,
                framebuffer_color_sample_counts: limits.framebuffer_color_sample_counts.as_raw()
                    as _,
                framebuffer_depth_sample_counts: limits.framebuffer_depth_sample_counts.as_raw()
                    as _,
                framebuffer_stencil_sample_counts: limits.framebuffer_stencil_sample_counts.as_raw()
                    as _,
                timestamp_compute_and_graphics: limits.timestamp_compute_and_graphics != 0,
                max_color_attachments: limits.max_color_attachments as _,
                buffer_image_granularity: limits.buffer_image_granularity,
                non_coherent_atom_size: limits.non_coherent_atom_size as _,
                max_sampler_anisotropy: limits.max_sampler_anisotropy,
                min_vertex_input_binding_stride_alignment: 1,
                max_bound_descriptor_sets: limits.max_bound_descriptor_sets as _,
                max_compute_shared_memory_size: limits.max_compute_shared_memory_size as _,
                max_compute_work_group_invocations: limits.max_compute_work_group_invocations as _,
                descriptor_limits: DescriptorLimits {
                    max_per_stage_descriptor_samplers: limits.max_per_stage_descriptor_samplers,
                    max_per_stage_descriptor_storage_buffers: limits
                        .max_per_stage_descriptor_storage_buffers,
                    max_per_stage_descriptor_uniform_buffers: limits
                        .max_per_stage_descriptor_uniform_buffers,
                    max_per_stage_descriptor_sampled_images: limits
                        .max_per_stage_descriptor_sampled_images,
                    max_per_stage_descriptor_storage_images: limits
                        .max_per_stage_descriptor_storage_images,
                    max_per_stage_descriptor_input_attachments: limits
                        .max_per_stage_descriptor_input_attachments,
                    max_per_stage_resources: limits.max_per_stage_resources,
                    max_descriptor_set_samplers: limits.max_descriptor_set_samplers,
                    max_descriptor_set_uniform_buffers: limits.max_descriptor_set_uniform_buffers,
                    max_descriptor_set_uniform_buffers_dynamic: limits
                        .max_descriptor_set_uniform_buffers_dynamic,
                    max_descriptor_set_storage_buffers: limits.max_descriptor_set_storage_buffers,
                    max_descriptor_set_storage_buffers_dynamic: limits
                        .max_descriptor_set_storage_buffers_dynamic,
                    max_descriptor_set_sampled_images: limits.max_descriptor_set_sampled_images,
                    max_descriptor_set_storage_images: limits.max_descriptor_set_storage_images,
                    max_descriptor_set_input_attachments: limits
                        .max_descriptor_set_input_attachments,
                },
                max_draw_indexed_index_value: limits.max_draw_indexed_index_value,
                max_draw_indirect_count: limits.max_draw_indirect_count,
                max_fragment_combined_output_resources: limits
                    .max_fragment_combined_output_resources
                    as _,
                max_fragment_dual_source_attachments: limits.max_fragment_dual_src_attachments as _,
                max_fragment_input_components: limits.max_fragment_input_components as _,
                max_fragment_output_attachments: limits.max_fragment_output_attachments as _,
                max_framebuffer_layers: limits.max_framebuffer_layers as _,
                max_geometry_input_components: limits.max_geometry_input_components as _,
                max_geometry_output_components: limits.max_geometry_output_components as _,
                max_geometry_output_vertices: limits.max_geometry_output_vertices as _,
                max_geometry_shader_invocations: limits.max_geometry_shader_invocations as _,
                max_geometry_total_output_components: limits.max_geometry_total_output_components
                    as _,
                max_memory_allocation_count: limits.max_memory_allocation_count as _,
                max_push_constants_size: limits.max_push_constants_size as _,
                max_sampler_allocation_count: limits.max_sampler_allocation_count as _,
                max_sampler_lod_bias: limits.max_sampler_lod_bias as _,
                max_storage_buffer_range: limits.max_storage_buffer_range as _,
                max_uniform_buffer_range: limits.max_uniform_buffer_range as _,
                min_memory_map_alignment: limits.min_memory_map_alignment,
                standard_sample_locations: limits.standard_sample_locations == ash::vk::TRUE,
            }
        };

        let mut descriptor_indexing_capabilities = hal::DescriptorIndexingProperties::default();
        let mut mesh_shader_capabilities = hal::MeshShaderProperties::default();
        let mut sampler_reduction_capabilities = hal::SamplerReductionProperties::default();
        let mut external_memory_limits = hal::ExternalMemoryLimits::default();

        if let Some(get_physical_device_properties) =
            self.instance.get_physical_device_properties.as_ref()
        {
            let mut descriptor_indexing_properties =
                vk::PhysicalDeviceDescriptorIndexingPropertiesEXT::builder();
            let mut mesh_shader_properties = vk::PhysicalDeviceMeshShaderPropertiesNV::builder();
            let mut sampler_reduction_properties =
                vk::PhysicalDeviceSamplerFilterMinmaxProperties::builder();
            let mut memory_host_properties =
                vk::PhysicalDeviceExternalMemoryHostPropertiesEXT::builder();

            let mut physical_device_properties2 = vk::PhysicalDeviceProperties2::builder()
                .push_next(&mut descriptor_indexing_properties)
                .push_next(&mut mesh_shader_properties)
                .push_next(&mut sampler_reduction_properties)
                .push_next(&mut memory_host_properties)
                .build();

            match get_physical_device_properties {
                ExtensionFn::Promoted => {
                    use ash::version::InstanceV1_1;
                    unsafe {
                        self.instance.inner.get_physical_device_properties2(
                            self.handle,
                            &mut physical_device_properties2,
                        );
                    }
                }
                ExtensionFn::Extension(get_physical_device_properties) => unsafe {
                    get_physical_device_properties.get_physical_device_properties2_khr(
                        self.handle,
                        &mut physical_device_properties2,
                    );
                },
            }

            descriptor_indexing_capabilities = hal::DescriptorIndexingProperties {
                shader_uniform_buffer_array_non_uniform_indexing_native:
                    descriptor_indexing_properties
                        .shader_uniform_buffer_array_non_uniform_indexing_native
                        == vk::TRUE,
                shader_sampled_image_array_non_uniform_indexing_native:
                    descriptor_indexing_properties
                        .shader_sampled_image_array_non_uniform_indexing_native
                        == vk::TRUE,
                shader_storage_buffer_array_non_uniform_indexing_native:
                    descriptor_indexing_properties
                        .shader_storage_buffer_array_non_uniform_indexing_native
                        == vk::TRUE,
                shader_storage_image_array_non_uniform_indexing_native:
                    descriptor_indexing_properties
                        .shader_storage_image_array_non_uniform_indexing_native
                        == vk::TRUE,
                shader_input_attachment_array_non_uniform_indexing_native:
                    descriptor_indexing_properties
                        .shader_input_attachment_array_non_uniform_indexing_native
                        == vk::TRUE,
                quad_divergent_implicit_lod: descriptor_indexing_properties
                    .quad_divergent_implicit_lod
                    == vk::TRUE,
            };

            mesh_shader_capabilities = hal::MeshShaderProperties {
                max_draw_mesh_tasks_count: mesh_shader_properties.max_draw_mesh_tasks_count,
                max_task_work_group_invocations: mesh_shader_properties
                    .max_task_work_group_invocations,
                max_task_work_group_size: mesh_shader_properties.max_task_work_group_size,
                max_task_total_memory_size: mesh_shader_properties.max_task_total_memory_size,
                max_task_output_count: mesh_shader_properties.max_task_output_count,
                max_mesh_work_group_invocations: mesh_shader_properties
                    .max_mesh_work_group_invocations,
                max_mesh_work_group_size: mesh_shader_properties.max_mesh_work_group_size,
                max_mesh_total_memory_size: mesh_shader_properties.max_mesh_total_memory_size,
                max_mesh_output_vertices: mesh_shader_properties.max_mesh_output_vertices,
                max_mesh_output_primitives: mesh_shader_properties.max_mesh_output_primitives,
                max_mesh_multiview_view_count: mesh_shader_properties.max_mesh_multiview_view_count,
                mesh_output_per_vertex_granularity: mesh_shader_properties
                    .mesh_output_per_vertex_granularity,
                mesh_output_per_primitive_granularity: mesh_shader_properties
                    .mesh_output_per_primitive_granularity,
            };

            sampler_reduction_capabilities = hal::SamplerReductionProperties {
                single_component_formats: sampler_reduction_properties
                    .filter_minmax_single_component_formats
                    == vk::TRUE,
                image_component_mapping: sampler_reduction_properties
                    .filter_minmax_image_component_mapping
                    == vk::TRUE,
            };

            external_memory_limits = ExternalMemoryLimits {
                min_imported_host_pointer_alignment: memory_host_properties
                    .min_imported_host_pointer_alignment,
            };
        }

        PhysicalDeviceProperties {
            limits,
            descriptor_indexing: descriptor_indexing_capabilities,
            mesh_shader: mesh_shader_capabilities,
            sampler_reduction: sampler_reduction_capabilities,
            performance_caveats: Default::default(),
            dynamic_pipeline_states: DynamicStates::all(),
            downlevel: DownlevelProperties::all_enabled(),
            external_memory_limits,
        }
    }

    fn is_valid_cache(&self, cache: &[u8]) -> bool {
        const HEADER_SIZE: usize = 16 + vk::UUID_SIZE;

        if cache.len() < HEADER_SIZE {
            warn!("Bad cache data length {:?}", cache.len());
            return false;
        }

        let header_len = u32::from_le_bytes([cache[0], cache[1], cache[2], cache[3]]);
        let header_version = u32::from_le_bytes([cache[4], cache[5], cache[6], cache[7]]);
        let vendor_id = u32::from_le_bytes([cache[8], cache[9], cache[10], cache[11]]);
        let device_id = u32::from_le_bytes([cache[12], cache[13], cache[14], cache[15]]);

        // header length
        if (header_len as usize) < HEADER_SIZE {
            warn!("Bad header length {:?}", header_len);
            return false;
        }

        // cache header version
        if header_version != vk::PipelineCacheHeaderVersion::ONE.as_raw() as u32 {
            warn!("Unsupported cache header version: {:?}", header_version);
            return false;
        }

        // vendor id
        if vendor_id != self.device_info.properties.vendor_id {
            warn!(
                "Vendor ID mismatch. Device: {:?}, cache: {:?}.",
                self.device_info.properties.vendor_id, vendor_id,
            );
            return false;
        }

        // device id
        if device_id != self.device_info.properties.device_id {
            warn!(
                "Device ID mismatch. Device: {:?}, cache: {:?}.",
                self.device_info.properties.device_id, device_id,
            );
            return false;
        }

        if self.device_info.properties.pipeline_cache_uuid != cache[16..16 + vk::UUID_SIZE] {
            warn!(
                "Pipeline cache UUID mismatch. Device: {:?}, cache: {:?}.",
                self.device_info.properties.pipeline_cache_uuid,
                &cache[16..16 + vk::UUID_SIZE],
            );
            return false;
        }
        true
    }

    unsafe fn enumerate_displays(&self) -> Vec<display::Display<Backend>> {
        let display_extension = match self.instance.display {
            Some(ref display_extension) => display_extension,
            None => {
                error!("Direct display feature not supported");
                return Vec::new();
            }
        };

        let display_properties =
            match display_extension.get_physical_device_display_properties(self.handle) {
                Ok(display_properties) => display_properties,
                Err(err) => {
                    match err {
                        vk::Result::ERROR_OUT_OF_HOST_MEMORY
                        | vk::Result::ERROR_OUT_OF_DEVICE_MEMORY => error!(
                            "Error returned on `get_physical_device_display_properties`: {:#?}",
                            err
                        ),
                        err => error!(
                            "Unexpected error on `get_physical_device_display_properties`: {:#?}",
                            err
                        ),
                    }
                    return Vec::new();
                }
            };

        let mut displays = Vec::new();
        for display_property in display_properties {
            let supported_transforms = hal::display::SurfaceTransformFlags::from_bits(
                display_property.supported_transforms.as_raw(),
            )
            .unwrap();
            let display_name = if display_property.display_name.is_null() {
                None
            } else {
                Some(
                    std::ffi::CStr::from_ptr(display_property.display_name)
                        .to_str()
                        .unwrap()
                        .to_owned(),
                )
            };

            let display_info = display::DisplayInfo {
                name: display_name,
                physical_dimensions: (
                    display_property.physical_dimensions.width,
                    display_property.physical_dimensions.height,
                )
                    .into(),
                physical_resolution: (
                    display_property.physical_resolution.width,
                    display_property.physical_resolution.height,
                )
                    .into(),
                supported_transforms: supported_transforms,
                plane_reorder_possible: display_property.plane_reorder_possible == 1,
                persistent_content: display_property.persistent_content == 1,
            };

            let display_modes = match display_extension
                .get_display_mode_properties(self.handle, display_property.display)
            {
                Ok(display_modes) => display_modes,
                Err(err) => {
                    match err {
                        vk::Result::ERROR_OUT_OF_HOST_MEMORY
                        | vk::Result::ERROR_OUT_OF_DEVICE_MEMORY => error!(
                            "Error returned on `get_display_mode_properties`: {:#?}",
                            err
                        ),
                        err => error!(
                            "Unexpected error on `get_display_mode_properties`: {:#?}",
                            err
                        ),
                    }
                    return Vec::new();
                }
            }
            .iter()
            .map(|display_mode_properties| display::DisplayMode {
                handle: native::DisplayMode(display_mode_properties.display_mode),
                resolution: (
                    display_mode_properties.parameters.visible_region.width,
                    display_mode_properties.parameters.visible_region.height,
                ),
                refresh_rate: display_mode_properties.parameters.refresh_rate,
            })
            .collect();

            let display = display::Display {
                handle: native::Display(display_property.display),
                info: display_info,
                modes: display_modes,
            };

            displays.push(display);
        }
        return displays;
    }

    unsafe fn enumerate_compatible_planes(
        &self,
        display: &display::Display<Backend>,
    ) -> Vec<display::Plane> {
        let display_extension = match self.instance.display {
            Some(ref display_extension) => display_extension,
            None => {
                error!("Direct display feature not supported");
                return Vec::new();
            }
        };

        match display_extension.get_physical_device_display_plane_properties(self.handle) {
            Ok(planes_properties) => {
                let mut planes = Vec::new();
                for index in 0..planes_properties.len() {
                    let compatible_displays = match display_extension
                        .get_display_plane_supported_displays(self.handle, index as u32)
                    {
                        Ok(compatible_displays) => compatible_displays,
                        Err(err) => {
                            match err {
                                vk::Result::ERROR_OUT_OF_HOST_MEMORY | vk::Result::ERROR_OUT_OF_DEVICE_MEMORY =>
                                    error!("Error returned on `get_display_plane_supported_displays`: {:#?}",err),
                                err=>error!("Unexpected error on `get_display_plane_supported_displays`: {:#?}",err)
                            }
                            return Vec::new();
                        }
                    };
                    if compatible_displays.contains(&display.handle.0) {
                        planes.push(display::Plane {
                            handle: index as u32,
                            z_index: planes_properties[index].current_stack_index,
                        });
                    }
                }
                planes
            }
            Err(err) => {
                match err {
                    vk::Result::ERROR_OUT_OF_HOST_MEMORY
                    | vk::Result::ERROR_OUT_OF_DEVICE_MEMORY => error!(
                        "Error returned on `get_physical_device_display_plane_properties`: {:#?}",
                        err
                    ),
                    err => error!(
                        "Unexpected error on `get_physical_device_display_plane_properties`: {:#?}",
                        err
                    ),
                }
                Vec::new()
            }
        }
    }

    unsafe fn create_display_mode(
        &self,
        display: &display::Display<Backend>,
        resolution: (u32, u32),
        refresh_rate: u32,
    ) -> Result<display::DisplayMode<Backend>, display::DisplayModeError> {
        let display_extension = self.instance.display.as_ref().unwrap();

        let display_mode_ci = vk::DisplayModeCreateInfoKHR::builder()
            .parameters(vk::DisplayModeParametersKHR {
                visible_region: vk::Extent2D {
                    width: resolution.0,
                    height: resolution.1,
                },
                refresh_rate: refresh_rate,
            })
            .build();

        match display_extension.create_display_mode(
            self.handle,
            display.handle.0,
            &display_mode_ci,
            None,
        ) {
            Ok(display_mode_handle) => Ok(display::DisplayMode {
                handle: native::DisplayMode(display_mode_handle),
                resolution: resolution,
                refresh_rate: refresh_rate,
            }),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => return Err(OutOfMemory::Host.into()),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => return Err(OutOfMemory::Device.into()),
            Err(vk::Result::ERROR_INITIALIZATION_FAILED) => {
                return Err(display::DisplayModeError::UnsupportedDisplayMode.into())
            }
            Err(err) => panic!("Unexpected error on `create_display_mode`: {:#?}", err),
        }
    }

    unsafe fn create_display_plane<'a>(
        &self,
        display_mode: &'a display::DisplayMode<Backend>,
        plane: &'a display::Plane,
    ) -> Result<display::DisplayPlane<'a, Backend>, OutOfMemory> {
        let display_extension = self.instance.display.as_ref().unwrap();

        let display_plane_capabilities = match display_extension.get_display_plane_capabilities(
            self.handle,
            display_mode.handle.0,
            plane.handle,
        ) {
            Ok(display_plane_capabilities) => display_plane_capabilities,
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => return Err(OutOfMemory::Host.into()),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => return Err(OutOfMemory::Device.into()),
            Err(err) => panic!(
                "Unexpected error on `get_display_plane_capabilities`: {:#?}",
                err
            ),
        };

        let mut supported_alpha_capabilities = Vec::new();
        if display_plane_capabilities
            .supported_alpha
            .contains(vk::DisplayPlaneAlphaFlagsKHR::OPAQUE)
        {
            supported_alpha_capabilities.push(display::DisplayPlaneAlpha::Opaque);
        }
        if display_plane_capabilities
            .supported_alpha
            .contains(vk::DisplayPlaneAlphaFlagsKHR::GLOBAL)
        {
            supported_alpha_capabilities.push(display::DisplayPlaneAlpha::Global(1.0));
        }
        if display_plane_capabilities
            .supported_alpha
            .contains(vk::DisplayPlaneAlphaFlagsKHR::PER_PIXEL)
        {
            supported_alpha_capabilities.push(display::DisplayPlaneAlpha::PerPixel);
        }
        if display_plane_capabilities
            .supported_alpha
            .contains(vk::DisplayPlaneAlphaFlagsKHR::PER_PIXEL_PREMULTIPLIED)
        {
            supported_alpha_capabilities.push(display::DisplayPlaneAlpha::PerPixelPremultiplied);
        }

        Ok(display::DisplayPlane {
            plane: &plane,
            display_mode: &display_mode,
            supported_alpha: supported_alpha_capabilities,
            src_position: std::ops::Range {
                start: (
                    display_plane_capabilities.min_src_position.x,
                    display_plane_capabilities.min_src_position.x,
                )
                    .into(),
                end: (
                    display_plane_capabilities.max_src_position.x,
                    display_plane_capabilities.max_src_position.x,
                )
                    .into(),
            },
            src_extent: std::ops::Range {
                start: (
                    display_plane_capabilities.min_src_extent.width,
                    display_plane_capabilities.min_src_extent.height,
                )
                    .into(),
                end: (
                    display_plane_capabilities.max_src_extent.width,
                    display_plane_capabilities.max_src_extent.height,
                )
                    .into(),
            },
            dst_position: std::ops::Range {
                start: (
                    display_plane_capabilities.min_dst_position.x,
                    display_plane_capabilities.min_dst_position.x,
                )
                    .into(),
                end: (
                    display_plane_capabilities.max_dst_position.x,
                    display_plane_capabilities.max_dst_position.x,
                )
                    .into(),
            },
            dst_extent: std::ops::Range {
                start: (
                    display_plane_capabilities.min_dst_extent.width,
                    display_plane_capabilities.min_dst_extent.height,
                )
                    .into(),
                end: (
                    display_plane_capabilities.max_dst_extent.width,
                    display_plane_capabilities.max_dst_extent.height,
                )
                    .into(),
            },
        })
    }
}
