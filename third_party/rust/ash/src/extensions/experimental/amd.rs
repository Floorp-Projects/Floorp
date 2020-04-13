#![allow(clippy::unreadable_literal)]

/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 **********************************************************************************************************************/

use crate::vk::*;
use std::fmt;
use std::os::raw::*;

// Extension: `VK_AMD_gpa_interface`

#[repr(transparent)]
#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct GpaSqShaderStageFlags(pub(crate) Flags);
vk_bitflags_wrapped!(
    GpaSqShaderStageFlags,
    0b1111111111111111111111111111111,
    Flags
);
impl fmt::Debug for GpaSqShaderStageFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (GpaSqShaderStageFlags::PS.0, "PS"),
            (GpaSqShaderStageFlags::VS.0, "VS"),
            (GpaSqShaderStageFlags::GS.0, "GS"),
            (GpaSqShaderStageFlags::ES.0, "ES"),
            (GpaSqShaderStageFlags::HS.0, "HS"),
            (GpaSqShaderStageFlags::LS.0, "LS"),
            (GpaSqShaderStageFlags::CS.0, "CS"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl GpaSqShaderStageFlags {
    pub const PS: Self = GpaSqShaderStageFlags(0b1);
    pub const VS: Self = GpaSqShaderStageFlags(0b10);
    pub const GS: Self = GpaSqShaderStageFlags(0b100);
    pub const ES: Self = GpaSqShaderStageFlags(0b1000);
    pub const HS: Self = GpaSqShaderStageFlags(0b10000);
    pub const LS: Self = GpaSqShaderStageFlags(0b100000);
    pub const CS: Self = GpaSqShaderStageFlags(0b1000000);
}

impl StructureType {
    pub const PHYSICAL_DEVICE_GPA_FEATURES_AMD: Self = StructureType(1000133000);
    pub const PHYSICAL_DEVICE_GPA_PROPERTIES_AMD: Self = StructureType(1000133001);
    pub const GPA_SAMPLE_BEGIN_INFO_AMD: Self = StructureType(1000133002);
    pub const GPA_SESSION_CREATE_INFO_AMD: Self = StructureType(1000133003);
    pub const GPA_DEVICE_CLOCK_MODE_INFO_AMD: Self = StructureType(1000133004);
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Default)]
#[repr(transparent)]
pub struct GpaDeviceClockModeAmd(pub(crate) i32);
impl GpaDeviceClockModeAmd {
    pub fn from_raw(x: i32) -> Self {
        GpaDeviceClockModeAmd(x)
    }
    pub fn as_raw(self) -> i32 {
        self.0
    }
}
impl GpaDeviceClockModeAmd {
    pub const DEFAULT: Self = GpaDeviceClockModeAmd(0);
    pub const QUERY: Self = GpaDeviceClockModeAmd(1);
    pub const PROFILING: Self = GpaDeviceClockModeAmd(2);
    pub const MIN_MEMORY: Self = GpaDeviceClockModeAmd(3);
    pub const MIN_ENGINE: Self = GpaDeviceClockModeAmd(4);
    pub const PEAK: Self = GpaDeviceClockModeAmd(5);
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Default)]
#[repr(transparent)]
pub struct GpaPerfBlockAmd(pub(crate) i32);
impl GpaPerfBlockAmd {
    pub fn from_raw(x: i32) -> Self {
        GpaPerfBlockAmd(x)
    }
    pub fn as_raw(self) -> i32 {
        self.0
    }
}
impl GpaPerfBlockAmd {
    pub const CPF: Self = GpaPerfBlockAmd(0);
    pub const IA: Self = GpaPerfBlockAmd(1);
    pub const VGT: Self = GpaPerfBlockAmd(2);
    pub const PA: Self = GpaPerfBlockAmd(3);
    pub const SC: Self = GpaPerfBlockAmd(4);
    pub const SPI: Self = GpaPerfBlockAmd(5);
    pub const SQ: Self = GpaPerfBlockAmd(6);
    pub const SX: Self = GpaPerfBlockAmd(7);
    pub const TA: Self = GpaPerfBlockAmd(8);
    pub const TD: Self = GpaPerfBlockAmd(9);
    pub const TCP: Self = GpaPerfBlockAmd(10);
    pub const TCC: Self = GpaPerfBlockAmd(11);
    pub const TCA: Self = GpaPerfBlockAmd(12);
    pub const DB: Self = GpaPerfBlockAmd(13);
    pub const CB: Self = GpaPerfBlockAmd(14);
    pub const GDS: Self = GpaPerfBlockAmd(15);
    pub const SRBM: Self = GpaPerfBlockAmd(16);
    pub const GRBM: Self = GpaPerfBlockAmd(17);
    pub const GRBM_SE: Self = GpaPerfBlockAmd(18);
    pub const RLC: Self = GpaPerfBlockAmd(19);
    pub const DMA: Self = GpaPerfBlockAmd(20);
    pub const MC: Self = GpaPerfBlockAmd(21);
    pub const CPG: Self = GpaPerfBlockAmd(22);
    pub const CPC: Self = GpaPerfBlockAmd(23);
    pub const WD: Self = GpaPerfBlockAmd(24);
    pub const TCS: Self = GpaPerfBlockAmd(25);
    pub const ATC: Self = GpaPerfBlockAmd(26);
    pub const ATC_L2: Self = GpaPerfBlockAmd(27);
    pub const MC_VM_L2: Self = GpaPerfBlockAmd(28);
    pub const EA: Self = GpaPerfBlockAmd(29);
    pub const RPB: Self = GpaPerfBlockAmd(30);
    pub const RMI: Self = GpaPerfBlockAmd(31);
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Default)]
#[repr(transparent)]
pub struct GpaSampleTypeAmd(pub(crate) i32);
impl GpaSampleTypeAmd {
    pub fn from_raw(x: i32) -> Self {
        GpaSampleTypeAmd(x)
    }
    pub fn as_raw(self) -> i32 {
        self.0
    }
}
impl GpaSampleTypeAmd {
    pub const CUMULATIVE: Self = GpaSampleTypeAmd(0);
    pub const TRACE: Self = GpaSampleTypeAmd(1);
    pub const TIMING: Self = GpaSampleTypeAmd(2);
}

handle_nondispatchable!(GpaSessionAmd, UNKNOWN);

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct GpaSessionCreateInfoAmd {
    pub s_type: StructureType,
    pub p_next: *const c_void,
    pub secondary_copy_source: GpaSessionAmd,
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct GpaPerfBlockPropertiesAmd {
    pub block_type: GpaPerfBlockAmd,
    pub flags: Flags,
    pub instance_count: u32,
    pub max_event_id: u32,
    pub max_global_only_counters: u32,
    pub max_global_shared_counters: u32,
    pub max_streaming_counters: u32,
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct PhysicalDeviceGpaFeaturesAmd {
    pub s_type: StructureType,
    pub p_next: *const c_void,
    pub perf_counters: Bool32,
    pub streaming_perf_counters: Bool32,
    pub sq_thread_tracing: Bool32,
    pub clock_modes: Bool32,
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct PhysicalDeviceGpaPropertiesAmd {
    pub s_type: StructureType,
    pub p_next: *const c_void,
    pub flags: Flags,
    pub max_sqtt_se_buffer_size: DeviceSize,
    pub shader_engine_count: u32,
    pub perf_block_count: u32,
    pub p_perf_block_properties: *mut GpaPerfBlockPropertiesAmd,
}

impl ::std::default::Default for PhysicalDeviceGpaPropertiesAmd {
    fn default() -> PhysicalDeviceGpaPropertiesAmd {
        PhysicalDeviceGpaPropertiesAmd {
            s_type: StructureType::PHYSICAL_DEVICE_GPA_PROPERTIES_AMD,
            p_next: ::std::ptr::null_mut(),
            flags: Flags::default(),
            max_sqtt_se_buffer_size: DeviceSize::default(),
            shader_engine_count: u32::default(),
            perf_block_count: u32::default(),
            p_perf_block_properties: ::std::ptr::null_mut(),
        }
    }
}
impl PhysicalDeviceGpaPropertiesAmd {
    pub fn builder<'a>() -> PhysicalDeviceGpaPropertiesAmdBuilder<'a> {
        PhysicalDeviceGpaPropertiesAmdBuilder {
            inner: PhysicalDeviceGpaPropertiesAmd::default(),
            marker: ::std::marker::PhantomData,
        }
    }
}
pub struct PhysicalDeviceGpaPropertiesAmdBuilder<'a> {
    inner: PhysicalDeviceGpaPropertiesAmd,
    marker: ::std::marker::PhantomData<&'a ()>,
}
pub unsafe trait ExtendsPhysicalDeviceGpaPropertiesAmd {}
unsafe impl ExtendsPhysicalDeviceProperties2 for PhysicalDeviceGpaPropertiesAmd {}
impl<'a> ::std::ops::Deref for PhysicalDeviceGpaPropertiesAmdBuilder<'a> {
    type Target = PhysicalDeviceGpaPropertiesAmd;
    fn deref(&self) -> &Self::Target {
        &self.inner
    }
}
impl<'a> PhysicalDeviceGpaPropertiesAmdBuilder<'a> {
    pub fn next<T>(mut self, next: &'a mut T) -> PhysicalDeviceGpaPropertiesAmdBuilder<'a>
    where
        T: ExtendsPhysicalDeviceGpaPropertiesAmd,
    {
        self.inner.p_next = next as *mut T as *mut c_void;
        self
    }
    pub fn build(self) -> PhysicalDeviceGpaPropertiesAmd {
        self.inner
    }
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct GpaPerfCounterAmd {
    pub block_type: GpaPerfBlockAmd,
    pub block_instance: u32,
    pub event_id: u32,
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct GpaSampleBeginInfoAmd {
    pub s_type: StructureType,
    pub p_next: *const c_void,
    pub sample_type: GpaSampleTypeAmd,
    pub sample_internal_operations: Bool32,
    pub cache_flush_on_counter_collection: Bool32,
    pub sq_shader_mask_enable: Bool32,
    pub sq_shader_mask: GpaSqShaderStageFlags,
    pub perf_counter_count: u32,
    pub p_perf_counters: *const GpaPerfCounterAmd,
    pub streaming_perf_trace_sample_interval: u32,
    pub perf_counter_device_memory_limit: DeviceSize,
    pub sq_thread_trace_enable: Bool32,
    pub sq_thread_trace_suppress_instruction_tokens: Bool32,
    pub sq_thread_trace_device_memory_limit: DeviceSize,
    pub timing_pre_sample: PipelineStageFlags,
    pub timing_post_sample: PipelineStageFlags,
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct GpaDeviceClockModeInfoAmd {
    pub s_type: StructureType,
    pub p_next: *const c_void,
    pub clock_mode: GpaDeviceClockModeAmd,
    pub memory_clock_ratio_to_peak: f32,
    pub engine_clock_ratio_to_peak: f32,
}

#[allow(non_camel_case_types)]
pub type PFN_vkCreateGpaSessionAMD = extern "system" fn(
    device: Device,
    p_create_info: *const GpaSessionCreateInfoAmd,
    p_allocator: *const AllocationCallbacks,
    p_gpa_session: *mut GpaSessionAmd,
) -> Result;

#[allow(non_camel_case_types)]
pub type PFN_vkDestroyGpaSessionAMD = extern "system" fn(
    device: Device,
    gpa_session: GpaSessionAmd,
    p_allocator: *const AllocationCallbacks,
) -> c_void;

#[allow(non_camel_case_types)]
pub type PFN_vkSetGpaDeviceClockModeAMD =
    extern "system" fn(device: Device, p_info: *mut GpaDeviceClockModeInfoAmd) -> Result;

#[allow(non_camel_case_types)]
pub type PFN_vkCmdBeginGpaSessionAMD =
    extern "system" fn(commandBuffer: CommandBuffer, gpa_session: GpaSessionAmd) -> Result;

#[allow(non_camel_case_types)]
pub type PFN_vkCmdEndGpaSessionAMD =
    extern "system" fn(commandBuffer: CommandBuffer, gpa_session: GpaSessionAmd) -> Result;

#[allow(non_camel_case_types)]
pub type PFN_vkCmdBeginGpaSampleAMD = extern "system" fn(
    commandBuffer: CommandBuffer,
    gpa_session: GpaSessionAmd,
    p_gpa_sample_begin_info: *const GpaSampleBeginInfoAmd,
    p_sample_id: *mut u32,
) -> Result;

#[allow(non_camel_case_types)]
pub type PFN_vkCmdEndGpaSampleAMD = extern "system" fn(
    commandBuffer: CommandBuffer,
    gpa_session: GpaSessionAmd,
    sample_id: u32,
) -> c_void;

#[allow(non_camel_case_types)]
pub type PFN_vkGetGpaSessionStatusAMD =
    extern "system" fn(device: Device, gpaSession: GpaSessionAmd) -> Result;

#[allow(non_camel_case_types)]
pub type PFN_vkGetGpaSessionResultsAMD = extern "system" fn(
    device: Device,
    gpaSession: GpaSessionAmd,
    sample_id: u32,
    p_size_in_bytes: *mut usize,
    p_data: *mut c_void,
) -> Result;

#[allow(non_camel_case_types)]
pub type PFN_vkResetGpaSessionAMD =
    extern "system" fn(device: Device, gpaSession: GpaSessionAmd) -> Result;

#[allow(non_camel_case_types)]
pub type PFN_vkCmdCopyGpaSessionResultsAMD =
    extern "system" fn(commandBuffer: CommandBuffer, gpaSession: GpaSessionAmd) -> c_void;

pub struct AmdGpaInterfaceFn {
    pub create_gpa_session: PFN_vkCreateGpaSessionAMD,
    pub destroy_gpa_session: PFN_vkDestroyGpaSessionAMD,
    pub set_gpa_device_clock_mode: PFN_vkSetGpaDeviceClockModeAMD,
    pub cmd_begin_gpa_session: PFN_vkCmdBeginGpaSessionAMD,
    pub cmd_end_gpa_session: PFN_vkCmdEndGpaSessionAMD,
    pub cmd_begin_gpa_sample: PFN_vkCmdBeginGpaSampleAMD,
    pub cmd_end_gpa_sample: PFN_vkCmdEndGpaSampleAMD,
    pub get_gpa_session_status: PFN_vkGetGpaSessionStatusAMD,
    pub get_gpa_session_results: PFN_vkGetGpaSessionResultsAMD,
    pub reset_gpa_session: PFN_vkResetGpaSessionAMD,
    pub cmd_copy_gpa_session_results: PFN_vkCmdCopyGpaSessionResultsAMD,
}
unsafe impl Send for AmdGpaInterfaceFn {}
unsafe impl Sync for AmdGpaInterfaceFn {}

impl ::std::clone::Clone for AmdGpaInterfaceFn {
    fn clone(&self) -> Self {
        AmdGpaInterfaceFn {
            create_gpa_session: self.create_gpa_session,
            destroy_gpa_session: self.destroy_gpa_session,
            set_gpa_device_clock_mode: self.set_gpa_device_clock_mode,
            cmd_begin_gpa_session: self.cmd_begin_gpa_session,
            cmd_end_gpa_session: self.cmd_end_gpa_session,
            cmd_begin_gpa_sample: self.cmd_begin_gpa_sample,
            cmd_end_gpa_sample: self.cmd_end_gpa_sample,
            get_gpa_session_status: self.get_gpa_session_status,
            get_gpa_session_results: self.get_gpa_session_results,
            reset_gpa_session: self.reset_gpa_session,
            cmd_copy_gpa_session_results: self.cmd_copy_gpa_session_results,
        }
    }
}

impl AmdGpaInterfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdGpaInterfaceFn {
            create_gpa_session: unsafe {
                extern "system" fn create_gpa_session_amd(
                    _device: Device,
                    _p_create_info: *const GpaSessionCreateInfoAmd,
                    _p_allocator: *const AllocationCallbacks,
                    _p_gpa_session: *mut GpaSessionAmd,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_gpa_session_amd)
                    ))
                }
                let raw_name = stringify!(vkCreateGpaSessionAMD);
                let cname = ::std::ffi::CString::new(raw_name).unwrap();
                let val = _f(&cname);
                if val.is_null() {
                    create_gpa_session_amd
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_gpa_session: unsafe {
                extern "system" fn destroy_gpa_session_amd(
                    _device: Device,
                    _gpa_session: GpaSessionAmd,
                    _p_allocator: *const AllocationCallbacks,
                ) -> c_void {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(destroy_gpa_session_amd)
                    ))
                }
                let raw_name = stringify!(vkDestroyGpaSessionAMD);
                let cname = ::std::ffi::CString::new(raw_name).unwrap();
                let val = _f(&cname);
                if val.is_null() {
                    destroy_gpa_session_amd
                } else {
                    ::std::mem::transmute(val)
                }
            },
            set_gpa_device_clock_mode: unsafe {
                extern "system" fn set_gpa_device_clock_mode_amd(
                    _device: Device,
                    _p_info: *mut GpaDeviceClockModeInfoAmd,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(set_gpa_device_clock_mode_amd)
                    ))
                }
                let raw_name = stringify!(vkSetGpaDeviceClockModeAMD);
                let cname = ::std::ffi::CString::new(raw_name).unwrap();
                let val = _f(&cname);
                if val.is_null() {
                    set_gpa_device_clock_mode_amd
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_begin_gpa_session: unsafe {
                extern "system" fn cmd_begin_gpa_session_amd(
                    _command_buffer: CommandBuffer,
                    _gpa_session: GpaSessionAmd,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_begin_gpa_session_amd)
                    ))
                }
                let raw_name = stringify!(vkCmdBeginGpaSessionAMD);
                let cname = ::std::ffi::CString::new(raw_name).unwrap();
                let val = _f(&cname);
                if val.is_null() {
                    cmd_begin_gpa_session_amd
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_end_gpa_session: unsafe {
                extern "system" fn cmd_end_gpa_session_amd(
                    _command_buffer: CommandBuffer,
                    _gpa_session: GpaSessionAmd,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_end_gpa_session_amd)
                    ))
                }
                let raw_name = stringify!(vkCmdEndGpaSessionAMD);
                let cname = ::std::ffi::CString::new(raw_name).unwrap();
                let val = _f(&cname);
                if val.is_null() {
                    cmd_end_gpa_session_amd
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_begin_gpa_sample: unsafe {
                extern "system" fn cmd_begin_gpa_sample_amd(
                    _command_buffer: CommandBuffer,
                    _gpa_session: GpaSessionAmd,
                    _p_gpa_sample_begin_info: *const GpaSampleBeginInfoAmd,
                    _p_sample_id: *mut u32,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_begin_gpa_sample_amd)
                    ))
                }
                let raw_name = stringify!(vkCmdBeginGpaSampleAMD);
                let cname = ::std::ffi::CString::new(raw_name).unwrap();
                let val = _f(&cname);
                if val.is_null() {
                    cmd_begin_gpa_sample_amd
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_end_gpa_sample: unsafe {
                extern "system" fn cmd_end_gpa_sample_amd(
                    _command_buffer: CommandBuffer,
                    _gpa_session: GpaSessionAmd,
                    _sample_id: u32,
                ) -> c_void {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_end_gpa_sample_amd)
                    ))
                }
                let raw_name = stringify!(vkCmdEndGpaSampleAMD);
                let cname = ::std::ffi::CString::new(raw_name).unwrap();
                let val = _f(&cname);
                if val.is_null() {
                    cmd_end_gpa_sample_amd
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_gpa_session_status: unsafe {
                extern "system" fn get_gpa_session_status_amd(
                    _device: Device,
                    _gpa_session: GpaSessionAmd,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_gpa_session_status_amd)
                    ))
                }
                let raw_name = stringify!(vkGetGpaSessionStatusAMD);
                let cname = ::std::ffi::CString::new(raw_name).unwrap();
                let val = _f(&cname);
                if val.is_null() {
                    get_gpa_session_status_amd
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_gpa_session_results: unsafe {
                extern "system" fn get_gpa_session_results_amd(
                    _device: Device,
                    _gpa_session: GpaSessionAmd,
                    _sample_id: u32,
                    _p_size_in_bytes: *mut usize,
                    _p_data: *mut c_void,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_gpa_session_results_amd)
                    ))
                }
                let raw_name = stringify!(vkGetGpaSessionResultsAMD);
                let cname = ::std::ffi::CString::new(raw_name).unwrap();
                let val = _f(&cname);
                if val.is_null() {
                    get_gpa_session_results_amd
                } else {
                    ::std::mem::transmute(val)
                }
            },
            reset_gpa_session: unsafe {
                extern "system" fn reset_gpa_session_amd(
                    _device: Device,
                    _gpa_session: GpaSessionAmd,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(reset_gpa_session_amd)
                    ))
                }
                let raw_name = stringify!(vkCmdEndGpaSampleAMD);
                let cname = ::std::ffi::CString::new(raw_name).unwrap();
                let val = _f(&cname);
                if val.is_null() {
                    reset_gpa_session_amd
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_copy_gpa_session_results: unsafe {
                extern "system" fn cmd_copy_gpa_session_results_amd(
                    _command_buffer: CommandBuffer,
                    _gpa_session: GpaSessionAmd,
                ) -> c_void {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_copy_gpa_session_results_amd)
                    ))
                }
                let raw_name = stringify!(vkCmdCopyGpaSessionResultsAMD);
                let cname = ::std::ffi::CString::new(raw_name).unwrap();
                let val = _f(&cname);
                if val.is_null() {
                    cmd_copy_gpa_session_results_amd
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    pub unsafe fn create_gpa_session(
        &self,
        device: Device,
        create_info: *const GpaSessionCreateInfoAmd,
        allocator: *const AllocationCallbacks,
        gpa_session: *mut GpaSessionAmd,
    ) -> Result {
        (self.create_gpa_session)(device, create_info, allocator, gpa_session)
    }
    pub unsafe fn destroy_gpa_session(
        &self,
        device: Device,
        gpa_session: GpaSessionAmd,
        allocator: *const AllocationCallbacks,
    ) -> c_void {
        (self.destroy_gpa_session)(device, gpa_session, allocator)
    }
}

// Extension: `VK_AMD_wave_limits`

impl StructureType {
    pub const WAVE_LIMIT_AMD: Self = StructureType(1000045000);
    pub const PHYSICAL_DEVICE_WAVE_LIMIT_PROPERTIES_AMD: Self = StructureType(1000045001);
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct PhysicalDeviceWaveLimitPropertiesAmd {
    pub s_type: StructureType,
    pub p_next: *const c_void,
    pub cu_count: u32,
    pub max_waves_per_cu: u32,
}

impl ::std::default::Default for PhysicalDeviceWaveLimitPropertiesAmd {
    fn default() -> PhysicalDeviceWaveLimitPropertiesAmd {
        PhysicalDeviceWaveLimitPropertiesAmd {
            s_type: StructureType::PHYSICAL_DEVICE_WAVE_LIMIT_PROPERTIES_AMD,
            p_next: ::std::ptr::null_mut(),
            cu_count: u32::default(),
            max_waves_per_cu: u32::default(),
        }
    }
}
impl PhysicalDeviceWaveLimitPropertiesAmd {
    pub fn builder<'a>() -> PhysicalDeviceWaveLimitPropertiesAmdBuilder<'a> {
        PhysicalDeviceWaveLimitPropertiesAmdBuilder {
            inner: PhysicalDeviceWaveLimitPropertiesAmd::default(),
            marker: ::std::marker::PhantomData,
        }
    }
}
pub struct PhysicalDeviceWaveLimitPropertiesAmdBuilder<'a> {
    inner: PhysicalDeviceWaveLimitPropertiesAmd,
    marker: ::std::marker::PhantomData<&'a ()>,
}
pub unsafe trait ExtendsPhysicalDeviceWaveLimitPropertiesAmd {}
unsafe impl ExtendsPhysicalDeviceProperties2 for PhysicalDeviceWaveLimitPropertiesAmd {}
impl<'a> ::std::ops::Deref for PhysicalDeviceWaveLimitPropertiesAmdBuilder<'a> {
    type Target = PhysicalDeviceWaveLimitPropertiesAmd;
    fn deref(&self) -> &Self::Target {
        &self.inner
    }
}
impl<'a> PhysicalDeviceWaveLimitPropertiesAmdBuilder<'a> {
    pub fn push_next<T>(
        mut self,
        next: &'a mut T,
    ) -> PhysicalDeviceWaveLimitPropertiesAmdBuilder<'a>
    where
        T: ExtendsPhysicalDeviceWaveLimitPropertiesAmd,
    {
        unsafe {
            let next_ptr = next as *mut T as *mut BaseOutStructure;
            let last_next = ptr_chain_iter(next).last().unwrap();
            (*last_next).p_next = self.inner.p_next as _;
            self.inner.p_next = next_ptr as _;
        }
        self
    }
    pub fn build(self) -> PhysicalDeviceWaveLimitPropertiesAmd {
        self.inner
    }
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct PipelineShaderStageCreateInfoWaveLimitAmd {
    pub s_type: StructureType,
    pub p_next: *const c_void,
    pub waves_per_cu: f32,
    pub cu_enable_mask: *mut u32,
}
