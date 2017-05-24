// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use std::default::Default;
use std::os::raw::{c_char, c_int, c_long, c_uint, c_void};
use std::ptr;

pub enum Context {}
pub enum Stream {}

// These need to match cubeb_sample_format
pub const SAMPLE_S16LE: c_int = 0;
pub const SAMPLE_S16BE: c_int = 1;
pub const SAMPLE_FLOAT32LE: c_int = 2;
pub const SAMPLE_FLOAT32BE: c_int = 3;
pub type SampleFormat = c_int;

#[cfg(target_endian = "little")]
pub const SAMPLE_S16NE: c_int = SAMPLE_S16LE;
#[cfg(target_endian = "little")]
pub const SAMPLE_FLOAT32NE: c_int = SAMPLE_FLOAT32LE;
#[cfg(target_endian = "big")]
pub const SAMPLE_S16NE: c_int = SAMPLE_S16BE;
#[cfg(target_endian = "big")]
pub const SAMPLE_FLOAT32NE: c_int = SAMPLE_FLOAT32BE;

pub type DeviceId = *const c_void;

// These need to match cubeb_channel_layout
pub const LAYOUT_UNDEFINED: c_int = 0;
pub const LAYOUT_DUAL_MONO: c_int = 1;
pub const LAYOUT_DUAL_MONO_LFE: c_int = 2;
pub const LAYOUT_MONO: c_int = 3;
pub const LAYOUT_MONO_LFE: c_int = 4;
pub const LAYOUT_STEREO: c_int = 5;
pub const LAYOUT_STEREO_LFE: c_int = 6;
pub const LAYOUT_3F: c_int = 7;
pub const LAYOUT_3F_LFE: c_int = 8;
pub const LAYOUT_2F1: c_int = 9;
pub const LAYOUT_2F1_LFE: c_int = 10;
pub const LAYOUT_3F1: c_int = 11;
pub const LAYOUT_3F1_LFE: c_int = 12;
pub const LAYOUT_2F2: c_int = 13;
pub const LAYOUT_2F2_LFE: c_int = 14;
pub const LAYOUT_3F2: c_int = 15;
pub const LAYOUT_3F2_LFE: c_int = 16;
pub const LAYOUT_3F3R_LFE: c_int = 17;
pub const LAYOUT_3F4_LFE: c_int = 18;
pub const LAYOUT_MAX: c_int = 19;
pub type ChannelLayout = c_int;

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct StreamParams {
    pub format: SampleFormat,
    pub rate: u32,
    pub channels: u32,
    pub layout: ChannelLayout,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct Device {
    pub output_name: *mut c_char,
    pub input_name: *mut c_char,
}

impl Default for Device {
    fn default() -> Self {
        Device {
            output_name: ptr::null_mut(),
            input_name: ptr::null_mut(),
        }
    }
}

// These need to match cubeb_state
pub const STATE_STARTED: c_int = 0;
pub const STATE_STOPPED: c_int = 1;
pub const STATE_DRAINED: c_int = 2;
pub const STATE_ERROR: c_int = 3;
pub type State = c_int;

pub const OK: i32 = 0;
pub const ERROR: i32 = -1;
pub const ERROR_INVALID_FORMAT: i32 = -2;
pub const ERROR_INVALID_PARAMETER: i32 = -3;
pub const ERROR_NOT_SUPPORTED: i32 = -4;
pub const ERROR_DEVICE_UNAVAILABLE: i32 = -5;

// These need to match cubeb_device_type
bitflags! {
    #[repr(C)]
    pub flags DeviceType : u32 {
        const DEVICE_TYPE_UNKNOWN = 0b00,
        const DEVICE_TYPE_INPUT = 0b01,
        const DEVICE_TYPE_OUTPUT = 0b10,
        const DEVICE_TYPE_ALL = 0b11,
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum DeviceState {
    Disabled = 0,
    Unplugged = 1,
    Enabled = 2,
}

// These need to match cubeb_device_fmt
bitflags! {
    #[repr(C)]
    pub flags DeviceFmt: u32 {
        const DEVICE_FMT_S16LE = 0x0010,
        const DEVICE_FMT_S16BE = 0x0020,
        const DEVICE_FMT_F32LE = 0x1000,
        const DEVICE_FMT_F32BE = 0x2000,
        const DEVICE_FMT_S16_MASK = DEVICE_FMT_S16LE.bits | DEVICE_FMT_S16BE.bits,
        const DEVICE_FMT_F32_MASK = DEVICE_FMT_F32LE.bits | DEVICE_FMT_F32BE.bits,
        const DEVICE_FMT_ALL = DEVICE_FMT_S16_MASK.bits | DEVICE_FMT_F32_MASK.bits,
    }
}

// Ideally these would be defined as part of `flags DeviceFmt` but
// that doesn't work with current bitflags crate.
#[cfg(target_endian = "little")]
pub const CUBEB_FMT_S16NE: DeviceFmt = DEVICE_FMT_S16LE;
#[cfg(target_endian = "little")]
pub const CUBEB_FMT_F32NE: DeviceFmt = DEVICE_FMT_F32LE;
#[cfg(target_endian = "big")]
pub const CUBEB_FMT_S16NE: DeviceFmt = DEVICE_FMT_S16BE;
#[cfg(target_endian = "big")]
pub const CUBEB_FMT_F32NE: DeviceFmt = DEVICE_FMT_F32BE;

// These need to match cubeb_device_pref
bitflags! {
    #[repr(C)]
    pub flags DevicePref : u32 {
        const DEVICE_PREF_MULTIMEDIA = 0x1,
        const DEVICE_PREF_VOICE = 0x2,
        const DEVICE_PREF_NOTIFICATION = 0x4,
        const DEVICE_PREF_ALL = 0xF
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct DeviceInfo {
    pub devid: DeviceId,
    pub device_id: *const c_char,
    pub friendly_name: *const c_char,
    pub group_id: *const c_char,
    pub vendor_name: *const c_char,
    pub devtype: DeviceType,
    pub state: DeviceState,
    pub preferred: DevicePref,
    pub format: DeviceFmt,
    pub default_format: DeviceFmt,
    pub max_channels: u32,
    pub default_rate: u32,
    pub max_rate: u32,
    pub min_rate: u32,
    pub latency_lo: u32,
    pub latency_hi: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct DeviceCollection {
    /// Array of device info.
    pub device: *const DeviceInfo,
    /// Device count in collection.
    pub count: usize,
}

pub type DataCallback = Option<unsafe extern "C" fn(stream: *mut Stream,
                                                    user_ptr: *mut c_void,
                                                    input_buffer: *const c_void,
                                                    output_buffer: *mut c_void,
                                                    nframes: c_long)
                                                    -> c_long>;
pub type StateCallback = Option<unsafe extern "C" fn(stream: *mut Stream, user_ptr: *mut c_void, state: State)>;
pub type DeviceChangedCallback = Option<unsafe extern "C" fn(user_ptr: *mut c_void)>;
pub type DeviceCollectionChangedCallback = Option<unsafe extern "C" fn(context: *mut Context, user_ptr: *mut c_void)>;

pub type StreamInitFn = Option<unsafe extern "C" fn(context: *mut Context,
                                                    stream: *mut *mut Stream,
                                                    stream_name: *const c_char,
                                                    input_device: DeviceId,
                                                    input_stream_params: *mut StreamParams,
                                                    output_device: DeviceId,
                                                    output_stream_params: *mut StreamParams,
                                                    latency: u32,
                                                    data_callback: DataCallback,
                                                    state_callback: StateCallback,
                                                    user_ptr: *mut c_void)
                                                    -> i32>;

pub type RegisterDeviceCollectionChangedFn = Option<unsafe extern "C" fn(context: *mut Context,
                                                                         devtype: DeviceType,
                                                                         callback: DeviceCollectionChangedCallback,
                                                                         user_ptr: *mut c_void)
                                                                         -> i32>;

#[repr(C)]
pub struct Ops {
    pub init: Option<unsafe extern "C" fn(context: *mut *mut Context, context_name: *const c_char) -> i32>,
    pub get_backend_id: Option<unsafe extern "C" fn(context: *mut Context) -> *const c_char>,
    pub get_max_channel_count: Option<unsafe extern "C" fn(context: *mut Context, max_channels: *mut u32) -> i32>,
    pub get_min_latency: Option<unsafe extern "C" fn(context: *mut Context,
                                                     params: StreamParams,
                                                     latency_ms: *mut u32)
                                                     -> i32>,
    pub get_preferred_sample_rate: Option<unsafe extern "C" fn(context: *mut Context, rate: *mut u32) -> i32>,
    pub get_preferred_channel_layout:
        Option<unsafe extern "C" fn(context: *mut Context, layout: *mut ChannelLayout) -> i32>,
    pub enumerate_devices: Option<unsafe extern "C" fn(context: *mut Context,
                                                       devtype: DeviceType,
                                                       collection: *mut DeviceCollection)
                                                       -> i32>,
    pub device_collection_destroy:
        Option<unsafe extern "C" fn(context: *mut Context, collection: *mut DeviceCollection) -> i32>,
    pub destroy: Option<unsafe extern "C" fn(context: *mut Context)>,
    pub stream_init: StreamInitFn,
    pub stream_destroy: Option<unsafe extern "C" fn(stream: *mut Stream)>,
    pub stream_start: Option<unsafe extern "C" fn(stream: *mut Stream) -> i32>,
    pub stream_stop: Option<unsafe extern "C" fn(stream: *mut Stream) -> i32>,
    pub stream_get_position: Option<unsafe extern "C" fn(stream: *mut Stream, position: *mut u64) -> i32>,
    pub stream_get_latency: Option<unsafe extern "C" fn(stream: *mut Stream, latency: *mut u32) -> i32>,
    pub stream_set_volume: Option<unsafe extern "C" fn(stream: *mut Stream, volumes: f32) -> i32>,
    pub stream_set_panning: Option<unsafe extern "C" fn(stream: *mut Stream, panning: f32) -> i32>,
    pub stream_get_current_device: Option<unsafe extern "C" fn(stream: *mut Stream, device: *mut *const Device) -> i32>,
    pub stream_device_destroy: Option<unsafe extern "C" fn(stream: *mut Stream, device: *mut Device) -> i32>,
    pub stream_register_device_changed_callback:
        Option<unsafe extern "C" fn(stream: *mut Stream,
                                    device_changed_callback: DeviceChangedCallback)
                                    -> i32>,
    pub register_device_collection_changed: RegisterDeviceCollectionChangedFn,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct LayoutMap {
    pub name: *const c_char,
    pub channels: u32,
    pub layout: ChannelLayout,
}

// cubeb_mixer.h

// These need to match cubeb_channel
pub const CHANNEL_INVALID: c_int = -1;
pub const CHANNEL_MONO: c_int = 0;
pub const CHANNEL_LEFT: c_int = 1;
pub const CHANNEL_RIGHT: c_int = 2;
pub const CHANNEL_CENTER: c_int = 3;
pub const CHANNEL_LS: c_int = 4;
pub const CHANNEL_RS: c_int = 5;
pub const CHANNEL_RLS: c_int = 6;
pub const CHANNEL_RCENTER: c_int = 7;
pub const CHANNEL_RRS: c_int = 8;
pub const CHANNEL_LFE: c_int = 9;
pub const CHANNEL_MAX: c_int = 256;
pub type Channel = c_int;

#[repr(C)]
pub struct ChannelMap {
    pub channels: c_uint,
    pub map: [Channel; 256],
}
impl ::std::default::Default for ChannelMap {
    fn default() -> Self {
        ChannelMap {
            channels: 0,
            map: unsafe { ::std::mem::zeroed() },
        }
    }
}

extern "C" {
    pub fn cubeb_channel_map_to_layout(channel_map: *const ChannelMap) -> ChannelLayout;
    pub fn cubeb_should_upmix(stream: *const StreamParams, mixer: *const StreamParams) -> bool;
    pub fn cubeb_should_downmix(stream: *const StreamParams, mixer: *const StreamParams) -> bool;
    pub fn cubeb_downmix_float(input: *const f32,
                               inframes: c_long,
                               output: *mut f32,
                               in_channels: u32,
                               out_channels: u32,
                               in_layout: ChannelLayout,
                               out_layout: ChannelLayout);
    pub fn cubeb_upmix_float(input: *const f32,
                             inframes: c_long,
                             output: *mut f32,
                             in_channels: u32,
                             out_channels: u32);
}

#[test]
fn bindgen_test_layout_stream_params() {
    assert_eq!(::std::mem::size_of::<StreamParams>(),
               16usize,
               concat!("Size of: ", stringify!(StreamParams)));
    assert_eq!(::std::mem::align_of::<StreamParams>(),
               4usize,
               concat!("Alignment of ", stringify!(StreamParams)));
    assert_eq!(unsafe { &(*(0 as *const StreamParams)).format as *const _ as usize },
               0usize,
               concat!("Alignment of field: ",
                       stringify!(StreamParams),
                       "::",
                       stringify!(format)));
    assert_eq!(unsafe { &(*(0 as *const StreamParams)).rate as *const _ as usize },
               4usize,
               concat!("Alignment of field: ",
                       stringify!(StreamParams),
                       "::",
                       stringify!(rate)));
    assert_eq!(unsafe { &(*(0 as *const StreamParams)).channels as *const _ as usize },
               8usize,
               concat!("Alignment of field: ",
                       stringify!(StreamParams),
                       "::",
                       stringify!(channels)));
    assert_eq!(unsafe { &(*(0 as *const StreamParams)).layout as *const _ as usize },
               12usize,
               concat!("Alignment of field: ",
                       stringify!(StreamParams),
                       "::",
                       stringify!(layout)));
}

#[test]
fn bindgen_test_layout_cubeb_device() {
    assert_eq!(::std::mem::size_of::<Device>(),
               16usize,
               concat!("Size of: ", stringify!(Device)));
    assert_eq!(::std::mem::align_of::<Device>(),
               8usize,
               concat!("Alignment of ", stringify!(Device)));
    assert_eq!(unsafe { &(*(0 as *const Device)).output_name as *const _ as usize },
               0usize,
               concat!("Alignment of field: ",
                       stringify!(Device),
                       "::",
                       stringify!(output_name)));
    assert_eq!(unsafe { &(*(0 as *const Device)).input_name as *const _ as usize },
               8usize,
               concat!("Alignment of field: ",
                       stringify!(Device),
                       "::",
                       stringify!(input_name)));
}

#[test]
fn bindgen_test_layout_cubeb_device_info() {
    assert_eq!(::std::mem::size_of::<DeviceInfo>(),
               88usize,
               concat!("Size of: ", stringify!(DeviceInfo)));
    assert_eq!(::std::mem::align_of::<DeviceInfo>(),
               8usize,
               concat!("Alignment of ", stringify!(DeviceInfo)));
    assert_eq!(unsafe { &(*(0 as *const DeviceInfo)).devid as *const _ as usize },
               0usize,
               concat!("Alignment of field: ",
                       stringify!(DeviceInfo),
                       "::",
                       stringify!(devid)));
    assert_eq!(unsafe { &(*(0 as *const DeviceInfo)).device_id as *const _ as usize },
               8usize,
               concat!("Alignment of field: ",
                       stringify!(DeviceInfo),
                       "::",
                       stringify!(device_id)));
    assert_eq!(unsafe { &(*(0 as *const DeviceInfo)).friendly_name as *const _ as usize },
               16usize,
               concat!("Alignment of field: ",
                       stringify!(DeviceInfo),
                       "::",
                       stringify!(friendly_name)));
    assert_eq!(unsafe { &(*(0 as *const DeviceInfo)).group_id as *const _ as usize },
               24usize,
               concat!("Alignment of field: ",
                       stringify!(DeviceInfo),
                       "::",
                       stringify!(group_id)));
    assert_eq!(unsafe { &(*(0 as *const DeviceInfo)).vendor_name as *const _ as usize },
               32usize,
               concat!("Alignment of field: ",
                       stringify!(DeviceInfo),
                       "::",
                       stringify!(vendor_name)));
    assert_eq!(unsafe { &(*(0 as *const DeviceInfo)).devtype as *const _ as usize },
               40usize,
               concat!("Alignment of field: ",
                       stringify!(DeviceInfo),
                       "::",
                       stringify!(type_)));
    assert_eq!(unsafe { &(*(0 as *const DeviceInfo)).state as *const _ as usize },
               44usize,
               concat!("Alignment of field: ",
                       stringify!(DeviceInfo),
                       "::",
                       stringify!(state)));
    assert_eq!(unsafe { &(*(0 as *const DeviceInfo)).preferred as *const _ as usize },
               48usize,
               concat!("Alignment of field: ",
                       stringify!(DeviceInfo),
                       "::",
                       stringify!(preferred)));
    assert_eq!(unsafe { &(*(0 as *const DeviceInfo)).format as *const _ as usize },
               52usize,
               concat!("Alignment of field: ",
                       stringify!(DeviceInfo),
                       "::",
                       stringify!(format)));
    assert_eq!(unsafe { &(*(0 as *const DeviceInfo)).default_format as *const _ as usize },
               56usize,
               concat!("Alignment of field: ",
                       stringify!(DeviceInfo),
                       "::",
                       stringify!(default_format)));
    assert_eq!(unsafe { &(*(0 as *const DeviceInfo)).max_channels as *const _ as usize },
               60usize,
               concat!("Alignment of field: ",
                       stringify!(DeviceInfo),
                       "::",
                       stringify!(max_channels)));
    assert_eq!(unsafe { &(*(0 as *const DeviceInfo)).default_rate as *const _ as usize },
               64usize,
               concat!("Alignment of field: ",
                       stringify!(DeviceInfo),
                       "::",
                       stringify!(default_rate)));
    assert_eq!(unsafe { &(*(0 as *const DeviceInfo)).max_rate as *const _ as usize },
               68usize,
               concat!("Alignment of field: ",
                       stringify!(DeviceInfo),
                       "::",
                       stringify!(max_rate)));
    assert_eq!(unsafe { &(*(0 as *const DeviceInfo)).min_rate as *const _ as usize },
               72usize,
               concat!("Alignment of field: ",
                       stringify!(DeviceInfo),
                       "::",
                       stringify!(min_rate)));
    assert_eq!(unsafe { &(*(0 as *const DeviceInfo)).latency_lo as *const _ as usize },
               76usize,
               concat!("Alignment of field: ",
                       stringify!(DeviceInfo),
                       "::",
                       stringify!(latency_lo)));
    assert_eq!(unsafe { &(*(0 as *const DeviceInfo)).latency_hi as *const _ as usize },
               80usize,
               concat!("Alignment of field: ",
                       stringify!(DeviceInfo),
                       "::",
                       stringify!(latency_hi)));
}

#[test]
fn bindgen_test_layout_cubeb_device_collection() {
    assert_eq!(::std::mem::size_of::<DeviceCollection>(),
               8usize,
               concat!("Size of: ", stringify!(DeviceCollection)));
    assert_eq!(::std::mem::align_of::<DeviceCollection>(),
               8usize,
               concat!("Alignment of ", stringify!(DeviceCollection)));
    assert_eq!(unsafe { &(*(0 as *const DeviceCollection)).count as *const _ as usize },
               0usize,
               concat!("Alignment of field: ",
                       stringify!(DeviceCollection),
                       "::",
                       stringify!(count)));
    assert_eq!(unsafe { &(*(0 as *const DeviceCollection)).device as *const _ as usize },
               8usize,
               concat!("Alignment of field: ",
                       stringify!(DeviceCollection),
                       "::",
                       stringify!(device)));

}
