// Copyright 2017 GFX developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

use super::*;

use block::{Block, ConcreteBlock};
use foreign_types::ForeignType;
use objc::runtime::{Object, NO, YES};

use std::{ffi::CStr, os::raw::c_char, path::Path, ptr};

#[allow(non_camel_case_types)]
#[repr(u64)]
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub enum MTLFeatureSet {
    iOS_GPUFamily1_v1 = 0,
    iOS_GPUFamily2_v1 = 1,
    iOS_GPUFamily1_v2 = 2,
    iOS_GPUFamily2_v2 = 3,
    iOS_GPUFamily3_v1 = 4,
    iOS_GPUFamily1_v3 = 5,
    iOS_GPUFamily2_v3 = 6,
    iOS_GPUFamily3_v2 = 7,
    iOS_GPUFamily1_v4 = 8,
    iOS_GPUFamily2_v4 = 9,
    iOS_GPUFamily3_v3 = 10,
    iOS_GPUFamily4_v1 = 11,
    iOS_GPUFamily1_v5 = 12,
    iOS_GPUFamily2_v5 = 13,
    iOS_GPUFamily3_v4 = 14,
    iOS_GPUFamily4_v2 = 15,
    iOS_GPUFamily5_v1 = 16,

    tvOS_GPUFamily1_v1 = 30000,
    tvOS_GPUFamily1_v2 = 30001,
    tvOS_GPUFamily1_v3 = 30002,
    tvOS_GPUFamily2_v1 = 30003,
    tvOS_GPUFamily1_v4 = 30004,
    tvOS_GPUFamily2_v2 = 30005,

    macOS_GPUFamily1_v1 = 10000,
    macOS_GPUFamily1_v2 = 10001,
    //macOS_ReadWriteTextureTier2 = 10002, TODO: Uncomment when feature tables updated
    macOS_GPUFamily1_v3 = 10003,
    macOS_GPUFamily1_v4 = 10004,
    macOS_GPUFamily2_v1 = 10005,
}

#[repr(i64)]
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub enum MTLGPUFamily {
    Common1 = 3001,
    Common2 = 3002,
    Common3 = 3003,
    Apple1 = 1001,
    Apple2 = 1002,
    Apple3 = 1003,
    Apple4 = 1004,
    Apple5 = 1005,
    Apple6 = 1006,
    Mac1 = 2001,
    Mac2 = 2002,
    MacCatalyst1 = 4001,
    MacCatalyst2 = 4002,
}

#[repr(u64)]
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub enum MTLDeviceLocation {
    BuiltIn = 0,
    Slot = 1,
    External = 2,
    Unspecified = u64::MAX,
}

bitflags! {
    pub struct PixelFormatCapabilities: u32 {
        const Filter = 1 << 0;
        const Write = 1 << 1;
        const Color = 1 << 2;
        const Blend = 1 << 3;
        const Msaa = 1 << 4;
        const Resolve = 1 << 5;
    }
}

#[allow(non_camel_case_types)]
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
enum OS {
    iOS,
    tvOS,
    macOS,
}

const KB: u32 = 1024;
const MB: u32 = 1024 * KB;
const GB: u32 = 1024 * MB;

impl MTLFeatureSet {
    fn os(&self) -> OS {
        let value = *self as u64;
        if value < 10_000 {
            OS::iOS
        } else if value < 20_000 {
            OS::macOS
        } else if value >= 30_000 || value < 40_000 {
            OS::tvOS
        } else {
            unreachable!()
        }
    }

    // returns the minor version on macos
    fn os_version(&self) -> u32 {
        use MTLFeatureSet::*;
        match self {
            iOS_GPUFamily1_v1 | iOS_GPUFamily2_v1 => 8,
            iOS_GPUFamily1_v2 | iOS_GPUFamily2_v2 | iOS_GPUFamily3_v1 => 9,
            iOS_GPUFamily1_v3 | iOS_GPUFamily2_v3 | iOS_GPUFamily3_v2 => 10,
            iOS_GPUFamily1_v4 | iOS_GPUFamily2_v4 | iOS_GPUFamily3_v3 | iOS_GPUFamily4_v1 => 11,
            iOS_GPUFamily1_v5 | iOS_GPUFamily2_v5 | iOS_GPUFamily3_v4 | iOS_GPUFamily4_v2
            | iOS_GPUFamily5_v1 => 12,
            tvOS_GPUFamily1_v1 => 9,
            tvOS_GPUFamily1_v2 => 10,
            tvOS_GPUFamily1_v3 | tvOS_GPUFamily2_v1 => 11,
            tvOS_GPUFamily1_v4 | tvOS_GPUFamily2_v2 => 12,
            macOS_GPUFamily1_v1 => 11,
            macOS_GPUFamily1_v2 => 12,
            macOS_GPUFamily1_v3 => 13,
            macOS_GPUFamily1_v4 | macOS_GPUFamily2_v1 => 14,
        }
    }

    fn gpu_family(&self) -> u32 {
        use MTLFeatureSet::*;
        match self {
            iOS_GPUFamily1_v1 | iOS_GPUFamily1_v2 | iOS_GPUFamily1_v3 | iOS_GPUFamily1_v4
            | iOS_GPUFamily1_v5 | tvOS_GPUFamily1_v1 | tvOS_GPUFamily1_v2 | tvOS_GPUFamily1_v3
            | tvOS_GPUFamily1_v4 | macOS_GPUFamily1_v1 | macOS_GPUFamily1_v2
            | macOS_GPUFamily1_v3 | macOS_GPUFamily1_v4 => 1,
            iOS_GPUFamily2_v1 | iOS_GPUFamily2_v2 | iOS_GPUFamily2_v3 | iOS_GPUFamily2_v4
            | iOS_GPUFamily2_v5 | tvOS_GPUFamily2_v1 | tvOS_GPUFamily2_v2 | macOS_GPUFamily2_v1 => {
                2
            }
            iOS_GPUFamily3_v1 | iOS_GPUFamily3_v2 | iOS_GPUFamily3_v3 | iOS_GPUFamily3_v4 => 3,
            iOS_GPUFamily4_v1 | iOS_GPUFamily4_v2 => 4,
            iOS_GPUFamily5_v1 => 5,
        }
    }

    fn version(&self) -> u32 {
        use MTLFeatureSet::*;
        match self {
            iOS_GPUFamily1_v1 | iOS_GPUFamily2_v1 | iOS_GPUFamily3_v1 | iOS_GPUFamily4_v1
            | iOS_GPUFamily5_v1 | macOS_GPUFamily1_v1 | macOS_GPUFamily2_v1
            | tvOS_GPUFamily1_v1 | tvOS_GPUFamily2_v1 => 1,
            iOS_GPUFamily1_v2 | iOS_GPUFamily2_v2 | iOS_GPUFamily3_v2 | iOS_GPUFamily4_v2
            | macOS_GPUFamily1_v2 | tvOS_GPUFamily1_v2 | tvOS_GPUFamily2_v2 => 2,
            iOS_GPUFamily1_v3 | iOS_GPUFamily2_v3 | iOS_GPUFamily3_v3 | macOS_GPUFamily1_v3
            | tvOS_GPUFamily1_v3 => 3,
            iOS_GPUFamily1_v4 | iOS_GPUFamily2_v4 | iOS_GPUFamily3_v4 | tvOS_GPUFamily1_v4
            | macOS_GPUFamily1_v4 => 4,
            iOS_GPUFamily1_v5 | iOS_GPUFamily2_v5 => 5,
        }
    }

    pub fn supports_metal_kit(&self) -> bool {
        true
    }

    pub fn supports_metal_performance_shaders(&self) -> bool {
        match self.os() {
            OS::iOS => self.gpu_family() >= 2,
            OS::tvOS => true,
            OS::macOS => self.os_version() >= 13,
        }
    }

    pub fn supports_programmable_blending(&self) -> bool {
        self.os() != OS::macOS
    }

    pub fn supports_pvrtc_pixel_formats(&self) -> bool {
        self.os() != OS::macOS
    }

    pub fn supports_eac_etc_pixel_formats(&self) -> bool {
        self.os() != OS::macOS
    }

    pub fn supports_astc_pixel_formats(&self) -> bool {
        match self.os() {
            OS::iOS => self.gpu_family() >= 2,
            OS::tvOS => true,
            OS::macOS => false,
        }
    }

    pub fn supports_linear_textures(&self) -> bool {
        self.os() != OS::macOS || self.os_version() >= 13
    }

    pub fn supports_bc_pixel_formats(&self) -> bool {
        self.os() == OS::macOS
    }

    pub fn supports_msaa_depth_resolve(&self) -> bool {
        match self.os() {
            OS::iOS => self.gpu_family() >= 3,
            OS::tvOS => self.gpu_family() >= 2,
            OS::macOS => false,
        }
    }

    pub fn supports_counting_occlusion_query(&self) -> bool {
        match self.os() {
            OS::iOS => self.gpu_family() >= 3,
            OS::tvOS => self.gpu_family() >= 2,
            OS::macOS => true,
        }
    }

    pub fn supports_base_vertex_instance_drawing(&self) -> bool {
        match self.os() {
            OS::iOS => self.gpu_family() >= 3,
            OS::tvOS => self.gpu_family() >= 2,
            OS::macOS => true,
        }
    }

    pub fn supports_indirect_buffers(&self) -> bool {
        match self.os() {
            OS::iOS => self.gpu_family() >= 3,
            OS::tvOS => self.gpu_family() >= 2,
            OS::macOS => true,
        }
    }

    pub fn supports_cube_map_texture_arrays(&self) -> bool {
        match self.os() {
            OS::iOS => self.gpu_family() >= 4,
            OS::tvOS => false,
            OS::macOS => true,
        }
    }

    pub fn supports_texture_barriers(&self) -> bool {
        self.os() == OS::macOS
    }

    pub fn supports_layered_rendering(&self) -> bool {
        match self.os() {
            OS::iOS => self.gpu_family() >= 5,
            OS::tvOS => false,
            OS::macOS => true,
        }
    }

    pub fn supports_tessellation(&self) -> bool {
        match self.os() {
            OS::iOS => self.gpu_family() >= 3 && self.os_version() >= 10,
            OS::tvOS => self.gpu_family() >= 2,
            OS::macOS => self.os_version() >= 12,
        }
    }

    pub fn supports_resource_heaps(&self) -> bool {
        match self.os() {
            OS::iOS => self.os_version() >= 10,
            OS::tvOS => self.os_version() >= 10,
            OS::macOS => self.os_version() >= 13,
        }
    }

    pub fn supports_memoryless_render_targets(&self) -> bool {
        match self.os() {
            OS::iOS => self.os_version() >= 10,
            OS::tvOS => self.os_version() >= 10,
            OS::macOS => false,
        }
    }

    pub fn supports_function_specialization(&self) -> bool {
        match self.os() {
            OS::iOS => self.os_version() >= 10,
            OS::tvOS => self.os_version() >= 10,
            OS::macOS => self.os_version() >= 12,
        }
    }

    pub fn supports_function_buffer_read_writes(&self) -> bool {
        match self.os() {
            OS::iOS => self.gpu_family() >= 3 && self.os_version() >= 10,
            OS::tvOS => self.gpu_family() >= 2,
            OS::macOS => self.os_version() >= 12,
        }
    }

    pub fn supports_function_texture_read_writes(&self) -> bool {
        match self.os() {
            OS::iOS => self.gpu_family() >= 4,
            OS::tvOS => false,
            OS::macOS => self.os_version() >= 12,
        }
    }

    pub fn supports_array_of_textures(&self) -> bool {
        match self.os() {
            OS::iOS => self.gpu_family() >= 3 && self.os_version() >= 10,
            OS::tvOS => self.gpu_family() >= 2,
            OS::macOS => self.os_version() >= 13,
        }
    }

    pub fn supports_array_of_samplers(&self) -> bool {
        match self.os() {
            OS::iOS => self.gpu_family() >= 3 && self.os_version() >= 11,
            OS::tvOS => self.gpu_family() >= 2,
            OS::macOS => self.os_version() >= 12,
        }
    }

    pub fn supports_stencil_texture_views(&self) -> bool {
        match self.os() {
            OS::iOS => self.os_version() >= 10,
            OS::tvOS => self.os_version() >= 10,
            OS::macOS => self.os_version() >= 12,
        }
    }

    pub fn supports_depth_16_pixel_format(&self) -> bool {
        self.os() == OS::macOS && self.os_version() >= 12
    }

    pub fn supports_extended_range_pixel_formats(&self) -> bool {
        match self.os() {
            OS::iOS => self.gpu_family() >= 3 && self.os_version() >= 10,
            OS::tvOS => self.gpu_family() >= 2,
            OS::macOS => false,
        }
    }

    pub fn supports_wide_color_pixel_format(&self) -> bool {
        match self.os() {
            OS::iOS => self.os_version() >= 11,
            OS::tvOS => self.os_version() >= 11,
            OS::macOS => self.os_version() >= 13,
        }
    }

    pub fn supports_combined_msaa_store_and_resolve_action(&self) -> bool {
        match self.os() {
            OS::iOS => self.gpu_family() >= 3 && self.os_version() >= 10,
            OS::tvOS => self.gpu_family() >= 2,
            OS::macOS => self.os_version() >= 12,
        }
    }

    pub fn supports_deferred_store_action(&self) -> bool {
        match self.os() {
            OS::iOS => self.os_version() >= 10,
            OS::tvOS => self.os_version() >= 10,
            OS::macOS => self.os_version() >= 12,
        }
    }

    pub fn supports_msaa_blits(&self) -> bool {
        match self.os() {
            OS::iOS => self.os_version() >= 10,
            OS::tvOS => self.os_version() >= 10,
            OS::macOS => true,
        }
    }

    pub fn supports_srgb_writes(&self) -> bool {
        match self.os() {
            OS::iOS => self.gpu_family() >= 3 || (self.gpu_family() >= 2 && self.version() >= 3),
            OS::tvOS => self.os_version() >= 10,
            OS::macOS => self.gpu_family() >= 2,
        }
    }

    pub fn supports_16_bit_unsigned_integer_coordinates(&self) -> bool {
        match self.os() {
            OS::iOS => self.os_version() >= 10,
            OS::tvOS => self.os_version() >= 10,
            OS::macOS => self.os_version() >= 12,
        }
    }

    pub fn supports_extract_insert_and_reverse_bits(&self) -> bool {
        match self.os() {
            OS::iOS => self.os_version() >= 10,
            OS::tvOS => self.os_version() >= 10,
            OS::macOS => self.os_version() >= 12,
        }
    }

    pub fn supports_simd_barrier(&self) -> bool {
        match self.os() {
            OS::iOS => self.os_version() >= 10,
            OS::tvOS => self.os_version() >= 10,
            OS::macOS => self.os_version() >= 13,
        }
    }

    pub fn supports_sampler_max_anisotropy(&self) -> bool {
        match self.os() {
            OS::iOS => self.os_version() >= 10,
            OS::tvOS => self.os_version() >= 10,
            OS::macOS => self.os_version() >= 13,
        }
    }

    pub fn supports_sampler_lod_clamp(&self) -> bool {
        match self.os() {
            OS::iOS => self.os_version() >= 10,
            OS::tvOS => self.os_version() >= 10,
            OS::macOS => self.os_version() >= 13,
        }
    }

    pub fn supports_border_color(&self) -> bool {
        self.os() == OS::macOS && self.os_version() >= 12
    }

    pub fn supports_dual_source_blending(&self) -> bool {
        match self.os() {
            OS::iOS => self.os_version() >= 11,
            OS::tvOS => self.os_version() >= 11,
            OS::macOS => self.os_version() >= 12,
        }
    }

    pub fn supports_argument_buffers(&self) -> bool {
        match self.os() {
            OS::iOS => self.os_version() >= 11,
            OS::tvOS => self.os_version() >= 11,
            OS::macOS => self.os_version() >= 13,
        }
    }

    pub fn supports_programmable_sample_positions(&self) -> bool {
        match self.os() {
            OS::iOS => self.os_version() >= 11,
            OS::tvOS => self.os_version() >= 11,
            OS::macOS => self.os_version() >= 13,
        }
    }

    pub fn supports_uniform_type(&self) -> bool {
        match self.os() {
            OS::iOS => self.os_version() >= 11,
            OS::tvOS => self.os_version() >= 11,
            OS::macOS => self.os_version() >= 13,
        }
    }

    pub fn supports_imageblocks(&self) -> bool {
        self.os() == OS::iOS && self.gpu_family() >= 4
    }

    pub fn supports_tile_shaders(&self) -> bool {
        self.os() == OS::iOS && self.gpu_family() >= 4
    }

    pub fn supports_imageblock_sample_coverage_control(&self) -> bool {
        self.os() == OS::iOS && self.gpu_family() >= 4
    }

    pub fn supports_threadgroup_sharing(&self) -> bool {
        self.os() == OS::iOS && self.gpu_family() >= 4
    }

    pub fn supports_post_depth_coverage(&self) -> bool {
        self.os() == OS::iOS && self.gpu_family() >= 4
    }

    pub fn supports_quad_scoped_permute_operations(&self) -> bool {
        match self.os() {
            OS::iOS => self.gpu_family() >= 4,
            OS::tvOS => false,
            OS::macOS => self.os_version() >= 13,
        }
    }

    pub fn supports_raster_order_groups(&self) -> bool {
        match self.os() {
            OS::iOS => self.gpu_family() >= 4,
            OS::tvOS => false,
            OS::macOS => self.os_version() >= 13,
        }
    }

    pub fn supports_non_uniform_threadgroup_size(&self) -> bool {
        match self.os() {
            OS::iOS => self.gpu_family() >= 4,
            OS::tvOS => false,
            OS::macOS => self.os_version() >= 13,
        }
    }

    pub fn supports_multiple_viewports(&self) -> bool {
        match self.os() {
            OS::iOS => self.gpu_family() >= 5,
            OS::tvOS => false,
            OS::macOS => self.os_version() >= 13,
        }
    }

    pub fn supports_device_notifications(&self) -> bool {
        self.os() == OS::macOS && self.os_version() >= 13
    }

    pub fn supports_stencil_feedback(&self) -> bool {
        match self.os() {
            OS::iOS => self.gpu_family() >= 5,
            OS::tvOS => false,
            OS::macOS => self.gpu_family() >= 2,
        }
    }

    pub fn supports_stencil_resolve(&self) -> bool {
        match self.os() {
            OS::iOS => self.gpu_family() >= 5,
            OS::tvOS => false,
            OS::macOS => self.gpu_family() >= 2,
        }
    }

    pub fn supports_binary_archive(&self) -> bool {
        match self.os() {
            OS::iOS => self.gpu_family() >= 3,
            OS::tvOS => self.gpu_family() >= 3,
            OS::macOS => self.gpu_family() >= 1,
        }
    }

    pub fn max_vertex_attributes(&self) -> u32 {
        31
    }

    pub fn max_buffer_argument_entries(&self) -> u32 {
        31
    }

    pub fn max_texture_argument_entries(&self) -> u32 {
        if self.os() == OS::macOS {
            128
        } else {
            31
        }
    }

    pub fn max_sampler_state_argument_entries(&self) -> u32 {
        16
    }

    pub fn max_threadgroup_memory_argument_entries(&self) -> u32 {
        31
    }

    pub fn max_inlined_constant_data_buffers(&self) -> u32 {
        if self.os() == OS::macOS {
            14
        } else {
            31
        }
    }

    pub fn max_inline_constant_buffer_length(&self) -> u32 {
        4 * KB
    }

    pub fn max_threads_per_threadgroup(&self) -> u32 {
        if self.os() == OS::macOS || self.gpu_family() >= 4 {
            1024
        } else {
            512
        }
    }

    pub fn max_total_threadgroup_memory_allocation(&self) -> u32 {
        match (self.os(), self.gpu_family()) {
            (OS::iOS, 5) => 64 * KB,
            (OS::iOS, 4) => {
                if self.os_version() >= 12 {
                    64 * KB
                } else {
                    32 * KB
                }
            }
            (OS::iOS, 3) => 16 * KB,
            (OS::iOS, _) => 16 * KB - 32,
            (OS::tvOS, 1) => 16 * KB - 32,
            (OS::tvOS, _) => 16 * KB,
            (OS::macOS, _) => 32 * KB,
        }
    }

    pub fn max_total_tile_memory_allocation(&self) -> u32 {
        if self.os() == OS::iOS && self.gpu_family() == 4 {
            32 * KB
        } else {
            0
        }
    }

    pub fn threadgroup_memory_length_alignment(&self) -> u32 {
        16
    }

    pub fn max_constant_buffer_function_memory_allocation(&self) -> Option<u32> {
        if self.os() == OS::macOS {
            Some(64 * KB)
        } else {
            None
        }
    }

    pub fn max_fragment_inputs(&self) -> u32 {
        if self.os() == OS::macOS {
            32
        } else {
            60
        }
    }

    pub fn max_fragment_input_components(&self) -> u32 {
        if self.os() == OS::macOS {
            128
        } else {
            60
        }
    }

    pub fn max_function_constants(&self) -> u32 {
        match self.os() {
            OS::iOS if self.os_version() >= 11 => 65536,
            OS::tvOS if self.os_version() >= 10 => 65536,
            OS::macOS if self.os_version() >= 12 => 65536,
            _ => 0,
        }
    }

    pub fn max_tessellation_factor(&self) -> u32 {
        if self.supports_tessellation() {
            match self.os() {
                OS::iOS if self.gpu_family() >= 5 => 64,
                OS::iOS => 16,
                OS::tvOS => 16,
                OS::macOS => 64,
            }
        } else {
            0
        }
    }

    pub fn max_viewports_and_scissor_rectangles(&self) -> u32 {
        if self.supports_multiple_viewports() {
            16
        } else {
            1
        }
    }

    pub fn max_raster_order_groups(&self) -> u32 {
        if self.supports_raster_order_groups() {
            8
        } else {
            0
        }
    }

    pub fn max_buffer_length(&self) -> u32 {
        if self.os() == OS::macOS && self.os_version() >= 12 {
            1 * GB
        } else {
            256 * MB
        }
    }

    pub fn min_buffer_offset_alignment(&self) -> u32 {
        if self.os() == OS::macOS {
            256
        } else {
            4
        }
    }

    pub fn max_1d_texture_size(&self) -> u32 {
        match (self.os(), self.gpu_family()) {
            (OS::iOS, 1) | (OS::iOS, 2) => {
                if self.version() <= 2 {
                    4096
                } else {
                    8192
                }
            }
            (OS::tvOS, 1) => 8192,
            _ => 16384,
        }
    }

    pub fn max_2d_texture_size(&self) -> u32 {
        match (self.os(), self.gpu_family()) {
            (OS::iOS, 1) | (OS::iOS, 2) => {
                if self.version() <= 2 {
                    4096
                } else {
                    8192
                }
            }
            (OS::tvOS, 1) => 8192,
            _ => 16384,
        }
    }

    pub fn max_cube_map_texture_size(&self) -> u32 {
        match (self.os(), self.gpu_family()) {
            (OS::iOS, 1) | (OS::iOS, 2) => {
                if self.version() <= 2 {
                    4096
                } else {
                    8192
                }
            }
            (OS::tvOS, 1) => 8192,
            _ => 16384,
        }
    }

    pub fn max_3d_texture_size(&self) -> u32 {
        2048
    }

    pub fn max_array_layers(&self) -> u32 {
        2048
    }

    pub fn copy_texture_buffer_alignment(&self) -> u32 {
        match (self.os(), self.gpu_family()) {
            (OS::iOS, 1) | (OS::iOS, 2) | (OS::tvOS, 1) => 64,
            (OS::iOS, _) | (OS::tvOS, _) => 16,
            (OS::macOS, _) => 256,
        }
    }

    /// If this function returns `None` but linear textures are supported,
    /// the buffer alignment can be discovered via API query
    pub fn new_texture_buffer_alignment(&self) -> Option<u32> {
        match self.os() {
            OS::iOS => {
                if self.os_version() >= 11 {
                    None
                } else if self.gpu_family() == 3 {
                    Some(16)
                } else {
                    Some(64)
                }
            }
            OS::tvOS => {
                if self.os_version() >= 11 {
                    None
                } else {
                    Some(64)
                }
            }
            OS::macOS => None,
        }
    }

    pub fn max_color_render_targets(&self) -> u32 {
        if self.os() == OS::iOS && self.gpu_family() == 1 {
            4
        } else {
            8
        }
    }

    pub fn max_point_primitive_size(&self) -> u32 {
        511
    }

    pub fn max_total_color_render_target_size(&self) -> Option<u32> {
        match (self.os(), self.gpu_family()) {
            (OS::iOS, 1) => Some(128),
            (OS::iOS, 2) | (OS::iOS, 3) => Some(256),
            (OS::iOS, _) => Some(512),
            (OS::tvOS, _) => Some(256),
            (OS::macOS, _) => None,
        }
    }

    pub fn max_visibility_query_offset(&self) -> u32 {
        64 * KB - 8
    }

    pub fn a8_unorm_capabilities(&self) -> PixelFormatCapabilities {
        PixelFormatCapabilities::Filter
    }

    pub fn r8_unorm_capabilities(&self) -> PixelFormatCapabilities {
        PixelFormatCapabilities::all()
    }

    pub fn r8_unorm_srgb_capabilities(&self) -> PixelFormatCapabilities {
        if self.os() == OS::macOS {
            PixelFormatCapabilities::empty()
        } else if self.supports_srgb_writes() {
            PixelFormatCapabilities::all()
        } else {
            !PixelFormatCapabilities::Write
        }
    }

    pub fn r8_snorm_capabilities(&self) -> PixelFormatCapabilities {
        if self.os() == OS::iOS && self.gpu_family() == 1 {
            !PixelFormatCapabilities::Resolve
        } else {
            PixelFormatCapabilities::all()
        }
    }

    pub fn r8_uint_capabilities(&self) -> PixelFormatCapabilities {
        PixelFormatCapabilities::Write
            | PixelFormatCapabilities::Color
            | PixelFormatCapabilities::Msaa
    }

    pub fn r8_sint_capabilities(&self) -> PixelFormatCapabilities {
        PixelFormatCapabilities::Write
            | PixelFormatCapabilities::Color
            | PixelFormatCapabilities::Msaa
    }

    pub fn r16_unorm_capabilities(&self) -> PixelFormatCapabilities {
        if self.os() != OS::macOS {
            !PixelFormatCapabilities::Resolve
        } else {
            PixelFormatCapabilities::all()
        }
    }

    pub fn r16_snorm_capabilities(&self) -> PixelFormatCapabilities {
        if self.os() != OS::macOS {
            !PixelFormatCapabilities::Resolve
        } else {
            PixelFormatCapabilities::all()
        }
    }

    pub fn r16_uint_capabilities(&self) -> PixelFormatCapabilities {
        PixelFormatCapabilities::Write
            | PixelFormatCapabilities::Color
            | PixelFormatCapabilities::Msaa
    }

    pub fn r16_sint_capabilities(&self) -> PixelFormatCapabilities {
        PixelFormatCapabilities::Write
            | PixelFormatCapabilities::Color
            | PixelFormatCapabilities::Msaa
    }

    pub fn r16_float_capabilities(&self) -> PixelFormatCapabilities {
        PixelFormatCapabilities::all()
    }

    pub fn rg8_unorm_capabilities(&self) -> PixelFormatCapabilities {
        PixelFormatCapabilities::all()
    }

    pub fn rg8_unorm_srgb_capabilities(&self) -> PixelFormatCapabilities {
        if self.os() == OS::macOS {
            PixelFormatCapabilities::empty()
        } else if self.supports_srgb_writes() {
            PixelFormatCapabilities::all()
        } else {
            !PixelFormatCapabilities::Write
        }
    }

    pub fn rg8_snorm_capabilities(&self) -> PixelFormatCapabilities {
        if self.os() == OS::iOS && self.gpu_family() == 1 {
            !PixelFormatCapabilities::Resolve
        } else {
            PixelFormatCapabilities::all()
        }
    }

    pub fn rg8_uint_capabilities(&self) -> PixelFormatCapabilities {
        PixelFormatCapabilities::Write
            | PixelFormatCapabilities::Color
            | PixelFormatCapabilities::Msaa
    }

    pub fn rg8_sint_capabilities(&self) -> PixelFormatCapabilities {
        PixelFormatCapabilities::Write
            | PixelFormatCapabilities::Color
            | PixelFormatCapabilities::Msaa
    }

    pub fn b5_g6_r5_unorm_capabilities(&self) -> PixelFormatCapabilities {
        if self.os() == OS::macOS {
            PixelFormatCapabilities::empty()
        } else {
            !PixelFormatCapabilities::Write
        }
    }

    pub fn a1_bgr5_unorm_capabilities(&self) -> PixelFormatCapabilities {
        if self.os() == OS::macOS {
            PixelFormatCapabilities::empty()
        } else {
            !PixelFormatCapabilities::Write
        }
    }

    pub fn abgr4_unorm_capabilities(&self) -> PixelFormatCapabilities {
        if self.os() == OS::macOS {
            PixelFormatCapabilities::empty()
        } else {
            !PixelFormatCapabilities::Write
        }
    }

    pub fn bgr5_a1_unorm_capabilities(&self) -> PixelFormatCapabilities {
        if self.os() == OS::macOS {
            PixelFormatCapabilities::empty()
        } else {
            !PixelFormatCapabilities::Write
        }
    }

    pub fn r32_uint_capabilities(&self) -> PixelFormatCapabilities {
        if self.os() == OS::iOS && self.os_version() == 8 {
            PixelFormatCapabilities::Color
        } else if self.os() == OS::macOS {
            PixelFormatCapabilities::Color
                | PixelFormatCapabilities::Write
                | PixelFormatCapabilities::Msaa
        } else {
            PixelFormatCapabilities::Color | PixelFormatCapabilities::Write
        }
    }

    pub fn r32_sint_capabilities(&self) -> PixelFormatCapabilities {
        if self.os() == OS::iOS && self.os_version() == 8 {
            PixelFormatCapabilities::Color
        } else if self.os() == OS::macOS {
            PixelFormatCapabilities::Color
                | PixelFormatCapabilities::Write
                | PixelFormatCapabilities::Msaa
        } else {
            PixelFormatCapabilities::Color | PixelFormatCapabilities::Write
        }
    }

    pub fn r32_float_capabilities(&self) -> PixelFormatCapabilities {
        if self.os() == OS::iOS && self.os_version() == 8 {
            PixelFormatCapabilities::Color
                | PixelFormatCapabilities::Blend
                | PixelFormatCapabilities::Msaa
        } else if self.os() == OS::macOS {
            PixelFormatCapabilities::all()
        } else {
            PixelFormatCapabilities::Write
                | PixelFormatCapabilities::Color
                | PixelFormatCapabilities::Blend
                | PixelFormatCapabilities::Msaa
        }
    }

    pub fn rg16_unorm_capabilities(&self) -> PixelFormatCapabilities {
        if self.os() == OS::macOS {
            PixelFormatCapabilities::all()
        } else {
            !PixelFormatCapabilities::Resolve
        }
    }

    pub fn rg16_snorm_capabilities(&self) -> PixelFormatCapabilities {
        if self.os() == OS::macOS {
            PixelFormatCapabilities::all()
        } else {
            !PixelFormatCapabilities::Resolve
        }
    }

    pub fn rg16_uint_capabilities(&self) -> PixelFormatCapabilities {
        PixelFormatCapabilities::Write
            | PixelFormatCapabilities::Color
            | PixelFormatCapabilities::Msaa
    }

    pub fn rg16_sint_capabilities(&self) -> PixelFormatCapabilities {
        PixelFormatCapabilities::Write
            | PixelFormatCapabilities::Color
            | PixelFormatCapabilities::Msaa
    }

    pub fn rg16_float_capabilities(&self) -> PixelFormatCapabilities {
        PixelFormatCapabilities::all()
    }

    pub fn rgba8_unorm_capabilities(&self) -> PixelFormatCapabilities {
        PixelFormatCapabilities::all()
    }

    pub fn rgba8_unorm_srgb_capabilities(&self) -> PixelFormatCapabilities {
        if self.supports_srgb_writes() {
            PixelFormatCapabilities::all()
        } else {
            !PixelFormatCapabilities::Write
        }
    }

    pub fn rgba8_snorm_capabilities(&self) -> PixelFormatCapabilities {
        if self.os() == OS::iOS && self.gpu_family() == 1 {
            !PixelFormatCapabilities::Resolve
        } else {
            PixelFormatCapabilities::all()
        }
    }

    pub fn rgba8_uint_capabilities(&self) -> PixelFormatCapabilities {
        PixelFormatCapabilities::Write
            | PixelFormatCapabilities::Color
            | PixelFormatCapabilities::Msaa
    }

    pub fn rgba8_sint_capabilities(&self) -> PixelFormatCapabilities {
        PixelFormatCapabilities::Write
            | PixelFormatCapabilities::Color
            | PixelFormatCapabilities::Msaa
    }

    pub fn bgra8_unorm_capabilities(&self) -> PixelFormatCapabilities {
        PixelFormatCapabilities::all()
    }

    pub fn bgra8_unorm_srgb_capabilities(&self) -> PixelFormatCapabilities {
        if self.supports_srgb_writes() {
            PixelFormatCapabilities::all()
        } else {
            !PixelFormatCapabilities::Write
        }
    }

    pub fn rgb10_a2_unorm_capabilities(&self) -> PixelFormatCapabilities {
        let supports_writes = match self.os() {
            OS::iOS => self.gpu_family() >= 3,
            OS::tvOS => self.gpu_family() >= 2,
            OS::macOS => true,
        };
        if supports_writes {
            PixelFormatCapabilities::all()
        } else {
            !PixelFormatCapabilities::Write
        }
    }

    pub fn rgb10_a2_uint_capabilities(&self) -> PixelFormatCapabilities {
        let supports_writes = match self.os() {
            OS::iOS => self.gpu_family() >= 3,
            OS::tvOS => self.gpu_family() >= 2,
            OS::macOS => true,
        };
        if supports_writes {
            PixelFormatCapabilities::Write
                | PixelFormatCapabilities::Color
                | PixelFormatCapabilities::Msaa
        } else {
            PixelFormatCapabilities::Color | PixelFormatCapabilities::Msaa
        }
    }

    pub fn rg11_b10_float_capabilities(&self) -> PixelFormatCapabilities {
        let supports_writes = match self.os() {
            OS::iOS => self.gpu_family() >= 3,
            OS::tvOS => self.gpu_family() >= 2,
            OS::macOS => true,
        };
        if supports_writes {
            PixelFormatCapabilities::all()
        } else {
            !PixelFormatCapabilities::Write
        }
    }

    pub fn rgb9_e5_float_capabilities(&self) -> PixelFormatCapabilities {
        if self.os() == OS::macOS {
            PixelFormatCapabilities::Filter
        } else {
            let supports_writes = match self.os() {
                OS::iOS => self.gpu_family() >= 3,
                OS::tvOS => self.gpu_family() >= 2,
                OS::macOS => false,
            };
            if supports_writes {
                PixelFormatCapabilities::all()
            } else {
                !PixelFormatCapabilities::Write
            }
        }
    }

    pub fn rg32_uint_capabilities(&self) -> PixelFormatCapabilities {
        if self.os() == OS::iOS && self.os_version() == 8 {
            PixelFormatCapabilities::Color
        } else if self.os() == OS::macOS {
            PixelFormatCapabilities::Color
                | PixelFormatCapabilities::Write
                | PixelFormatCapabilities::Msaa
        } else {
            PixelFormatCapabilities::Color | PixelFormatCapabilities::Write
        }
    }

    pub fn rg32_sint_capabilities(&self) -> PixelFormatCapabilities {
        if self.os() == OS::iOS && self.os_version() == 8 {
            PixelFormatCapabilities::Color
        } else if self.os() == OS::macOS {
            PixelFormatCapabilities::Color
                | PixelFormatCapabilities::Write
                | PixelFormatCapabilities::Msaa
        } else {
            PixelFormatCapabilities::Color | PixelFormatCapabilities::Write
        }
    }

    pub fn rg32_float_capabilities(&self) -> PixelFormatCapabilities {
        if self.os() == OS::macOS {
            PixelFormatCapabilities::all()
        } else if self.os() == OS::iOS && self.os_version() == 8 {
            PixelFormatCapabilities::Color | PixelFormatCapabilities::Blend
        } else {
            PixelFormatCapabilities::Write
                | PixelFormatCapabilities::Color
                | PixelFormatCapabilities::Blend
        }
    }

    pub fn rgba16_unorm_capabilities(&self) -> PixelFormatCapabilities {
        if self.os() == OS::macOS {
            PixelFormatCapabilities::all()
        } else {
            !PixelFormatCapabilities::Write
        }
    }

    pub fn rgba16_snorm_capabilities(&self) -> PixelFormatCapabilities {
        if self.os() == OS::macOS {
            PixelFormatCapabilities::all()
        } else {
            !PixelFormatCapabilities::Write
        }
    }

    pub fn rgba16_uint_capabilities(&self) -> PixelFormatCapabilities {
        PixelFormatCapabilities::Write
            | PixelFormatCapabilities::Color
            | PixelFormatCapabilities::Msaa
    }

    pub fn rgba16_sint_capabilities(&self) -> PixelFormatCapabilities {
        PixelFormatCapabilities::Write
            | PixelFormatCapabilities::Color
            | PixelFormatCapabilities::Msaa
    }

    pub fn rgba16_float_capabilities(&self) -> PixelFormatCapabilities {
        PixelFormatCapabilities::all()
    }

    pub fn rgba32_uint_capabilities(&self) -> PixelFormatCapabilities {
        if self.os() == OS::iOS && self.os_version() == 8 {
            PixelFormatCapabilities::Color
        } else if self.os() == OS::macOS {
            PixelFormatCapabilities::Color
                | PixelFormatCapabilities::Write
                | PixelFormatCapabilities::Msaa
        } else {
            PixelFormatCapabilities::Color | PixelFormatCapabilities::Write
        }
    }

    pub fn rgba32_sint_capabilities(&self) -> PixelFormatCapabilities {
        if self.os() == OS::iOS && self.os_version() == 8 {
            PixelFormatCapabilities::Color
        } else if self.os() == OS::macOS {
            PixelFormatCapabilities::Color
                | PixelFormatCapabilities::Write
                | PixelFormatCapabilities::Msaa
        } else {
            PixelFormatCapabilities::Color | PixelFormatCapabilities::Write
        }
    }

    pub fn rgba32_float_capabilities(&self) -> PixelFormatCapabilities {
        if self.os() == OS::macOS {
            PixelFormatCapabilities::all()
        } else if self.os() == OS::iOS && self.version() == 8 {
            PixelFormatCapabilities::Color
        } else {
            PixelFormatCapabilities::Write | PixelFormatCapabilities::Color
        }
    }

    pub fn pvrtc_pixel_formats_capabilities(&self) -> PixelFormatCapabilities {
        if self.supports_pvrtc_pixel_formats() {
            PixelFormatCapabilities::Filter
        } else {
            PixelFormatCapabilities::empty()
        }
    }

    pub fn eac_etc_pixel_formats_capabilities(&self) -> PixelFormatCapabilities {
        if self.supports_eac_etc_pixel_formats() {
            PixelFormatCapabilities::Filter
        } else {
            PixelFormatCapabilities::empty()
        }
    }

    pub fn astc_pixel_formats_capabilities(&self) -> PixelFormatCapabilities {
        if self.supports_astc_pixel_formats() {
            PixelFormatCapabilities::Filter
        } else {
            PixelFormatCapabilities::empty()
        }
    }

    pub fn bc_pixel_formats_capabilities(&self) -> PixelFormatCapabilities {
        if self.supports_bc_pixel_formats() {
            PixelFormatCapabilities::Filter
        } else {
            PixelFormatCapabilities::empty()
        }
    }

    pub fn gbgr422_capabilities(&self) -> PixelFormatCapabilities {
        PixelFormatCapabilities::Filter
    }

    pub fn bgrg422_capabilities(&self) -> PixelFormatCapabilities {
        PixelFormatCapabilities::Filter
    }

    pub fn depth16_unorm_capabilities(&self) -> PixelFormatCapabilities {
        if self.supports_depth_16_pixel_format() {
            PixelFormatCapabilities::Filter
                | PixelFormatCapabilities::Msaa
                | PixelFormatCapabilities::Resolve
        } else {
            PixelFormatCapabilities::empty()
        }
    }

    pub fn depth32_float_capabilities(&self) -> PixelFormatCapabilities {
        if self.os() == OS::macOS {
            PixelFormatCapabilities::Filter
                | PixelFormatCapabilities::Msaa
                | PixelFormatCapabilities::Resolve
        } else if self.supports_msaa_depth_resolve() {
            PixelFormatCapabilities::Msaa | PixelFormatCapabilities::Resolve
        } else {
            PixelFormatCapabilities::Msaa
        }
    }

    pub fn stencil8_capabilities(&self) -> PixelFormatCapabilities {
        PixelFormatCapabilities::Msaa
    }

    pub fn depth24_unorm_stencil8_capabilities(&self) -> PixelFormatCapabilities {
        if self.os() == OS::macOS {
            PixelFormatCapabilities::Filter
                | PixelFormatCapabilities::Msaa
                | PixelFormatCapabilities::Resolve
        } else {
            PixelFormatCapabilities::empty()
        }
    }

    pub fn depth32_float_stencil8_capabilities(&self) -> PixelFormatCapabilities {
        if self.os() == OS::macOS {
            PixelFormatCapabilities::Filter
                | PixelFormatCapabilities::Msaa
                | PixelFormatCapabilities::Resolve
        } else if self.supports_msaa_depth_resolve() {
            PixelFormatCapabilities::Msaa | PixelFormatCapabilities::Resolve
        } else {
            PixelFormatCapabilities::Msaa
        }
    }

    pub fn x24_stencil8_capabilities(&self) -> PixelFormatCapabilities {
        if self.os() == OS::macOS {
            PixelFormatCapabilities::Msaa
        } else {
            PixelFormatCapabilities::empty()
        }
    }

    pub fn x32_stencil8_capabilities(&self) -> PixelFormatCapabilities {
        PixelFormatCapabilities::Msaa
    }

    pub fn bgra10_xr_capabilities(&self) -> PixelFormatCapabilities {
        if self.supports_extended_range_pixel_formats() {
            PixelFormatCapabilities::all()
        } else {
            PixelFormatCapabilities::empty()
        }
    }

    pub fn bgra10_xr_srgb_capabilities(&self) -> PixelFormatCapabilities {
        if self.supports_extended_range_pixel_formats() {
            PixelFormatCapabilities::all()
        } else {
            PixelFormatCapabilities::empty()
        }
    }

    pub fn bgr10_xr_capabilities(&self) -> PixelFormatCapabilities {
        if self.supports_extended_range_pixel_formats() {
            PixelFormatCapabilities::all()
        } else {
            PixelFormatCapabilities::empty()
        }
    }

    pub fn bgr10_xr_srgb_capabilities(&self) -> PixelFormatCapabilities {
        if self.supports_extended_range_pixel_formats() {
            PixelFormatCapabilities::all()
        } else {
            PixelFormatCapabilities::empty()
        }
    }

    pub fn bgr10_a2_unorm_capabilities(&self) -> PixelFormatCapabilities {
        if self.supports_wide_color_pixel_format() {
            if self.os() == OS::macOS {
                !PixelFormatCapabilities::Write
            } else {
                PixelFormatCapabilities::all()
            }
        } else {
            PixelFormatCapabilities::empty()
        }
    }
}

#[repr(u64)]
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub enum MTLArgumentBuffersTier {
    Tier1 = 0,
    Tier2 = 1,
}

#[repr(u64)]
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub enum MTLReadWriteTextureTier {
    TierNone = 0,
    Tier1 = 1,
    Tier2 = 2,
}

/// Only available on (macos(11.0), ios(14.0))
#[repr(u64)]
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub enum MTLCounterSamplingPoint {
    AtStageBoundary = 0,
    AtDrawBoundary = 1,
    AtDispatchBoundary = 2,
    AtTileDispatchBoundary = 3,
    AtBlitBoundary = 4,
}

/// Only available on (macos(11.0), macCatalyst(14.0), ios(13.0))
#[repr(u64)]
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub enum MTLSparseTextureRegionAlignmentMode {
    Outward = 0,
    Inward = 1,
}

bitflags! {
    struct MTLPipelineOption: NSUInteger {
        const None                      = 0;
        const ArgumentInfo              = 1 << 0;
        const BufferTypeInfo            = 1 << 1;
        /// Only available on (macos(11.0), ios(14.0))
        const FailOnBinaryArchiveMiss   = 1 << 2;
    }
}

#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
#[repr(C)]
pub struct MTLAccelerationStructureSizes {
    pub acceleration_structure_size: NSUInteger,
    pub build_scratch_buffer_size: NSUInteger,
    pub refit_scratch_buffer_size: NSUInteger,
}

#[link(name = "Metal", kind = "framework")]
extern "C" {
    fn MTLCreateSystemDefaultDevice() -> *mut MTLDevice;
    #[cfg(not(target_os = "ios"))]
    fn MTLCopyAllDevices() -> *mut Object; //TODO: Array
}

#[allow(non_camel_case_types)]
type dispatch_data_t = *mut Object;
#[allow(non_camel_case_types)]
pub type dispatch_queue_t = *mut Object;
#[allow(non_camel_case_types)]
type dispatch_block_t = *const Block<(), ()>;

#[cfg_attr(
    any(target_os = "macos", target_os = "ios"),
    link(name = "System", kind = "dylib")
)]
#[cfg_attr(
    not(any(target_os = "macos", target_os = "ios")),
    link(name = "dispatch", kind = "dylib")
)]
#[allow(improper_ctypes)]
extern "C" {
    static _dispatch_main_q: dispatch_queue_t;

    fn dispatch_data_create(
        buffer: *const std::ffi::c_void,
        size: crate::c_size_t,
        queue: dispatch_queue_t,
        destructor: dispatch_block_t,
    ) -> dispatch_data_t;
    fn dispatch_release(object: dispatch_data_t); // actually dispatch_object_t
}

/*type MTLNewLibraryCompletionHandler = extern fn(library: id, error: id);
type MTLNewRenderPipelineStateCompletionHandler = extern fn(renderPipelineState: id, error: id);
type MTLNewRenderPipelineStateWithReflectionCompletionHandler = extern fn(renderPipelineState: id, reflection: id, error: id);
type MTLNewComputePipelineStateCompletionHandler = extern fn(computePipelineState: id, error: id);
type MTLNewComputePipelineStateWithReflectionCompletionHandler = extern fn(computePipelineState: id, reflection: id, error: id);*/

pub enum MTLDevice {}

foreign_obj_type! {
    type CType = MTLDevice;
    pub struct Device;
    pub struct DeviceRef;
}

impl Device {
    pub fn system_default() -> Option<Self> {
        // `MTLCreateSystemDefaultDevice` may return null if Metal is not supported
        unsafe { MTLCreateSystemDefaultDevice().as_mut().map(|x| Self(x)) }
    }

    pub fn all() -> Vec<Self> {
        #[cfg(target_os = "ios")]
        {
            Self::system_default().into_iter().collect()
        }
        #[cfg(not(target_os = "ios"))]
        unsafe {
            let array = MTLCopyAllDevices();
            let count: NSUInteger = msg_send![array, count];
            let ret = (0..count)
                .map(|i| msg_send![array, objectAtIndex: i])
                // The elements of this array are references---we convert them to owned references
                // (which just means that we increment the reference count here, and it is
                // decremented in the `Drop` impl for `Device`)
                .map(|device: *mut Object| msg_send![device, retain])
                .collect();
            let () = msg_send![array, release];
            ret
        }
    }
}

impl DeviceRef {
    pub fn name(&self) -> &str {
        unsafe {
            let name = msg_send![self, name];
            crate::nsstring_as_str(name)
        }
    }

    #[cfg(feature = "private")]
    pub unsafe fn vendor(&self) -> &str {
        let name = msg_send![self, vendorName];
        crate::nsstring_as_str(name)
    }

    #[cfg(feature = "private")]
    pub unsafe fn family_name(&self) -> &str {
        let name = msg_send![self, familyName];
        crate::nsstring_as_str(name)
    }

    pub fn registry_id(&self) -> u64 {
        unsafe { msg_send![self, registryID] }
    }

    pub fn location(&self) -> MTLDeviceLocation {
        unsafe { msg_send![self, location] }
    }

    pub fn location_number(&self) -> NSUInteger {
        unsafe { msg_send![self, locationNumber] }
    }

    pub fn max_threadgroup_memory_length(&self) -> NSUInteger {
        unsafe { msg_send![self, maxThreadgroupMemoryLength] }
    }

    pub fn max_threads_per_threadgroup(&self) -> MTLSize {
        unsafe { msg_send![self, maxThreadsPerThreadgroup] }
    }

    pub fn is_low_power(&self) -> bool {
        unsafe {
            match msg_send![self, isLowPower] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    pub fn is_headless(&self) -> bool {
        unsafe {
            match msg_send![self, isHeadless] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    pub fn is_removable(&self) -> bool {
        unsafe {
            match msg_send![self, isRemovable] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    /// Only available on (macos(11.0), ios(14.0))
    pub fn supports_raytracing(&self) -> bool {
        unsafe {
            match msg_send![self, supportsRaytracing] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    pub fn has_unified_memory(&self) -> bool {
        unsafe {
            match msg_send![self, hasUnifiedMemory] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    pub fn recommended_max_working_set_size(&self) -> u64 {
        unsafe { msg_send![self, recommendedMaxWorkingSetSize] }
    }

    pub fn max_transfer_rate(&self) -> u64 {
        unsafe { msg_send![self, maxTransferRate] }
    }

    pub fn supports_feature_set(&self, feature: MTLFeatureSet) -> bool {
        unsafe {
            match msg_send![self, supportsFeatureSet: feature] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    pub fn supports_family(&self, family: MTLGPUFamily) -> bool {
        unsafe {
            match msg_send![self, supportsFamily: family] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    pub fn supports_vertex_amplification_count(&self, count: NSUInteger) -> bool {
        unsafe {
            match msg_send![self, supportsVertexAmplificationCount: count] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    pub fn supports_texture_sample_count(&self, count: NSUInteger) -> bool {
        unsafe {
            match msg_send![self, supportsTextureSampleCount: count] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    pub fn supports_shader_barycentric_coordinates(&self) -> bool {
        unsafe {
            match msg_send![self, supportsShaderBarycentricCoordinates] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    pub fn supports_function_pointers(&self) -> bool {
        unsafe {
            match msg_send![self, supportsFunctionPointers] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    /// Only available on (macos(11.0), ios(14.0))
    pub fn supports_dynamic_libraries(&self) -> bool {
        unsafe {
            match msg_send![self, supportsDynamicLibraries] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    /// Only available on (macos(11.0), ios(14.0))
    pub fn supports_counter_sampling(&self, sampling_point: MTLCounterSamplingPoint) -> bool {
        unsafe {
            match msg_send![self, supportsCounterSampling: sampling_point] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    pub fn d24_s8_supported(&self) -> bool {
        unsafe {
            match msg_send![self, isDepth24Stencil8PixelFormatSupported] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    pub fn new_fence(&self) -> Fence {
        unsafe { msg_send![self, newFence] }
    }

    pub fn new_command_queue(&self) -> CommandQueue {
        unsafe { msg_send![self, newCommandQueue] }
    }

    pub fn new_command_queue_with_max_command_buffer_count(
        &self,
        count: NSUInteger,
    ) -> CommandQueue {
        unsafe { msg_send![self, newCommandQueueWithMaxCommandBufferCount: count] }
    }

    pub fn new_default_library(&self) -> Library {
        unsafe { msg_send![self, newDefaultLibrary] }
    }

    pub fn new_library_with_source(
        &self,
        src: &str,
        options: &CompileOptionsRef,
    ) -> Result<Library, String> {
        let source = nsstring_from_str(src);
        unsafe {
            let mut err: *mut Object = ptr::null_mut();
            let library: *mut MTLLibrary = msg_send![self, newLibraryWithSource:source
                                                                        options:options
                                                                          error:&mut err];
            if !err.is_null() {
                let desc: *mut Object = msg_send![err, localizedDescription];
                let compile_error: *const c_char = msg_send![desc, UTF8String];
                let message = CStr::from_ptr(compile_error).to_string_lossy().into_owned();
                if library.is_null() {
                    return Err(message);
                } else {
                    warn!("Shader warnings: {}", message);
                }
            }

            assert!(!library.is_null());
            Ok(Library::from_ptr(library))
        }
    }

    pub fn new_library_with_file<P: AsRef<Path>>(&self, file: P) -> Result<Library, String> {
        let filename = nsstring_from_str(file.as_ref().to_string_lossy().as_ref());
        unsafe {
            let library: *mut MTLLibrary = try_objc! { err =>
                msg_send![self, newLibraryWithFile:filename.as_ref()
                                             error:&mut err]
            };
            Ok(Library::from_ptr(library))
        }
    }

    pub fn new_library_with_data(&self, library_data: &[u8]) -> Result<Library, String> {
        unsafe {
            let destructor_block = ConcreteBlock::new(|| {}).copy();
            let data = dispatch_data_create(
                library_data.as_ptr() as *const std::ffi::c_void,
                library_data.len() as crate::c_size_t,
                &_dispatch_main_q as *const _ as dispatch_queue_t,
                &*destructor_block.deref(),
            );

            let library: *mut MTLLibrary = try_objc! { err =>
                 msg_send![self, newLibraryWithData:data
                                              error:&mut err]
            };
            dispatch_release(data);
            Ok(Library::from_ptr(library))
        }
    }

    /// Only available on (macos(11.0), ios(14.0))
    pub fn new_dynamic_library(&self, library: &LibraryRef) -> Result<DynamicLibrary, String> {
        unsafe {
            let mut err: *mut Object = ptr::null_mut();
            let dynamic_library: *mut MTLDynamicLibrary = msg_send![self, newDynamicLibrary:library
                                                                                      error:&mut err];
            if !err.is_null() {
                // FIXME: copy pasta
                let desc: *mut Object = msg_send![err, localizedDescription];
                let compile_error: *const c_char = msg_send![desc, UTF8String];
                let message = CStr::from_ptr(compile_error).to_string_lossy().into_owned();
                Err(message)
            } else {
                Ok(DynamicLibrary::from_ptr(dynamic_library))
            }
        }
    }

    /// Only available on (macos(11.0), ios(14.0))
    pub fn new_dynamic_library_with_url(&self, url: &URLRef) -> Result<DynamicLibrary, String> {
        unsafe {
            let mut err: *mut Object = ptr::null_mut();
            let dynamic_library: *mut MTLDynamicLibrary = msg_send![self, newDynamicLibraryWithURL:url
                                                                                             error:&mut err];
            if !err.is_null() {
                // FIXME: copy pasta
                let desc: *mut Object = msg_send![err, localizedDescription];
                let compile_error: *const c_char = msg_send![desc, UTF8String];
                let message = CStr::from_ptr(compile_error).to_string_lossy().into_owned();
                Err(message)
            } else {
                Ok(DynamicLibrary::from_ptr(dynamic_library))
            }
        }
    }

    /// Only available on (macos(11.0), ios(14.0))
    pub fn new_binary_archive_with_descriptor(
        &self,
        descriptor: &BinaryArchiveDescriptorRef,
    ) -> Result<BinaryArchive, String> {
        unsafe {
            let mut err: *mut Object = ptr::null_mut();
            let binary_archive: *mut MTLBinaryArchive = msg_send![self, newBinaryArchiveWithDescriptor:descriptor
                                                     error:&mut err];
            if !err.is_null() {
                // TODO: copy pasta
                let desc: *mut Object = msg_send![err, localizedDescription];
                let c_msg: *const c_char = msg_send![desc, UTF8String];
                let message = CStr::from_ptr(c_msg).to_string_lossy().into_owned();
                Err(message)
            } else {
                Ok(BinaryArchive::from_ptr(binary_archive))
            }
        }
    }

    pub fn new_render_pipeline_state_with_reflection(
        &self,
        descriptor: &RenderPipelineDescriptorRef,
        reflection: &RenderPipelineReflectionRef,
    ) -> Result<RenderPipelineState, String> {
        unsafe {
            let reflection_options =
                MTLPipelineOption::ArgumentInfo | MTLPipelineOption::BufferTypeInfo;

            let pipeline_state: *mut MTLRenderPipelineState = try_objc! { err =>
                msg_send![self, newRenderPipelineStateWithDescriptor:descriptor
                                                             options:reflection_options
                                                          reflection:reflection
                                                               error:&mut err]
            };

            Ok(RenderPipelineState::from_ptr(pipeline_state))
        }
    }

    /// Useful for debugging binary archives.
    pub fn new_render_pipeline_state_with_fail_on_binary_archive_miss(
        &self,
        descriptor: &RenderPipelineDescriptorRef,
    ) -> Result<RenderPipelineState, String> {
        unsafe {
            let pipeline_options = MTLPipelineOption::FailOnBinaryArchiveMiss;

            let reflection: *mut MTLRenderPipelineReflection = std::ptr::null_mut();

            let pipeline_state: *mut MTLRenderPipelineState = try_objc! { err =>
                msg_send![self, newRenderPipelineStateWithDescriptor:descriptor
                                                             options:pipeline_options
                                                          reflection:reflection
                                                               error:&mut err]
            };

            Ok(RenderPipelineState::from_ptr(pipeline_state))
        }
    }

    pub fn new_render_pipeline_state(
        &self,
        descriptor: &RenderPipelineDescriptorRef,
    ) -> Result<RenderPipelineState, String> {
        unsafe {
            let pipeline_state: *mut MTLRenderPipelineState = try_objc! { err =>
                msg_send![self, newRenderPipelineStateWithDescriptor:descriptor
                                                               error:&mut err]
            };

            Ok(RenderPipelineState::from_ptr(pipeline_state))
        }
    }

    pub fn new_compute_pipeline_state_with_function(
        &self,
        function: &FunctionRef,
    ) -> Result<ComputePipelineState, String> {
        unsafe {
            let pipeline_state: *mut MTLComputePipelineState = try_objc! { err =>
                msg_send![self, newComputePipelineStateWithFunction:function
                                                              error:&mut err]
            };

            Ok(ComputePipelineState::from_ptr(pipeline_state))
        }
    }

    pub fn new_compute_pipeline_state(
        &self,
        descriptor: &ComputePipelineDescriptorRef,
    ) -> Result<ComputePipelineState, String> {
        unsafe {
            let pipeline_state: *mut MTLComputePipelineState = try_objc! { err =>
                msg_send![self, newComputePipelineStateWithDescriptor:descriptor
                                                                error:&mut err]
            };

            Ok(ComputePipelineState::from_ptr(pipeline_state))
        }
    }

    pub fn new_buffer(&self, length: u64, options: MTLResourceOptions) -> Buffer {
        unsafe {
            msg_send![self, newBufferWithLength:length
                                        options:options]
        }
    }

    pub fn new_buffer_with_bytes_no_copy(
        &self,
        bytes: *const std::ffi::c_void,
        length: NSUInteger,
        options: MTLResourceOptions,
        deallocator: Option<&Block<(*const std::ffi::c_void, NSUInteger), ()>>,
    ) -> Buffer {
        unsafe {
            msg_send![self, newBufferWithBytesNoCopy:bytes
                length:length
                options:options
                deallocator:deallocator]
        }
    }

    pub fn new_buffer_with_data(
        &self,
        bytes: *const std::ffi::c_void,
        length: NSUInteger,
        options: MTLResourceOptions,
    ) -> Buffer {
        unsafe {
            msg_send![self, newBufferWithBytes:bytes
                                        length:length
                                       options:options]
        }
    }

    pub fn new_texture(&self, descriptor: &TextureDescriptorRef) -> Texture {
        unsafe { msg_send![self, newTextureWithDescriptor: descriptor] }
    }

    pub fn new_sampler(&self, descriptor: &SamplerDescriptorRef) -> SamplerState {
        unsafe { msg_send![self, newSamplerStateWithDescriptor: descriptor] }
    }

    pub fn new_depth_stencil_state(
        &self,
        descriptor: &DepthStencilDescriptorRef,
    ) -> DepthStencilState {
        unsafe { msg_send![self, newDepthStencilStateWithDescriptor: descriptor] }
    }

    pub fn argument_buffers_support(&self) -> MTLArgumentBuffersTier {
        unsafe { msg_send![self, argumentBuffersSupport] }
    }

    pub fn read_write_texture_support(&self) -> MTLReadWriteTextureTier {
        unsafe { msg_send![self, readWriteTextureSupport] }
    }

    pub fn raster_order_groups_supported(&self) -> bool {
        unsafe {
            match msg_send![self, rasterOrderGroupsSupported] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    /// Only available on (macos(11.0), ios(14.0))
    pub fn supports_32bit_float_filtering(&self) -> bool {
        unsafe {
            match msg_send![self, supports32BitFloatFiltering] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    /// Only available on (macos(11.0), ios(14.0))
    pub fn supports_32bit_MSAA(&self) -> bool {
        unsafe {
            match msg_send![self, supports32BitMSAA] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    /// Only available on (macos(11.0), ios(14.0))
    pub fn supports_query_texture_LOD(&self) -> bool {
        unsafe {
            match msg_send![self, supportsQueryTextureLOD] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    /// Only available on (macos(11.0), ios(14.0))
    pub fn supports_BC_texture_compression(&self) -> bool {
        unsafe {
            match msg_send![self, supportsBCTextureCompression] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    /// Only available on (macos(11.0), ios(14.0))
    pub fn supports_pull_model_interpolation(&self) -> bool {
        unsafe {
            match msg_send![self, supportsPullModelInterpolation] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    pub fn new_argument_encoder(
        &self,
        arguments: &ArrayRef<ArgumentDescriptor>,
    ) -> ArgumentEncoder {
        unsafe { msg_send![self, newArgumentEncoderWithArguments: arguments] }
    }

    pub fn new_heap(&self, descriptor: &HeapDescriptorRef) -> Heap {
        unsafe { msg_send![self, newHeapWithDescriptor: descriptor] }
    }

    pub fn new_event(&self) -> Event {
        unsafe { msg_send![self, newEvent] }
    }

    pub fn new_shared_event(&self) -> SharedEvent {
        unsafe { msg_send![self, newSharedEvent] }
    }

    pub fn heap_buffer_size_and_align(
        &self,
        length: NSUInteger,
        options: MTLResourceOptions,
    ) -> MTLSizeAndAlign {
        unsafe { msg_send![self, heapBufferSizeAndAlignWithLength: length options: options] }
    }

    pub fn heap_texture_size_and_align(
        &self,
        descriptor: &TextureDescriptorRef,
    ) -> MTLSizeAndAlign {
        unsafe { msg_send![self, heapTextureSizeAndAlignWithDescriptor: descriptor] }
    }

    pub fn minimum_linear_texture_alignment_for_pixel_format(
        &self,
        format: MTLPixelFormat,
    ) -> NSUInteger {
        unsafe { msg_send![self, minimumLinearTextureAlignmentForPixelFormat: format] }
    }

    pub fn minimum_texture_buffer_alignment_for_pixel_format(
        &self,
        format: MTLPixelFormat,
    ) -> NSUInteger {
        unsafe { msg_send![self, minimumTextureBufferAlignmentForPixelFormat: format] }
    }

    pub fn max_argument_buffer_sampler_count(&self) -> NSUInteger {
        unsafe { msg_send![self, maxArgumentBufferSamplerCount] }
    }

    pub fn current_allocated_size(&self) -> NSUInteger {
        unsafe { msg_send![self, currentAllocatedSize] }
    }

    /// Only available on (macos(10.14), ios(12.0), tvos(12.0))
    pub fn max_buffer_length(&self) -> NSUInteger {
        unsafe { msg_send![self, maxBufferLength] }
    }
}
