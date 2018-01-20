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
pub type SampleFormat = c_int;
pub const SAMPLE_S16LE: SampleFormat = 0;
pub const SAMPLE_S16BE: SampleFormat = 1;
pub const SAMPLE_FLOAT32LE: SampleFormat = 2;
pub const SAMPLE_FLOAT32BE: SampleFormat = 3;

#[cfg(target_endian = "little")]
pub const SAMPLE_S16NE: SampleFormat = SAMPLE_S16LE;
#[cfg(target_endian = "little")]
pub const SAMPLE_FLOAT32NE: SampleFormat = SAMPLE_FLOAT32LE;
#[cfg(target_endian = "big")]
pub const SAMPLE_S16NE: SampleFormat = SAMPLE_S16BE;
#[cfg(target_endian = "big")]
pub const SAMPLE_FLOAT32NE: SampleFormat = SAMPLE_FLOAT32BE;

pub type DeviceId = *const c_void;

// These need to match cubeb_channel_layout
pub type ChannelLayout = c_int;
pub const LAYOUT_UNDEFINED: ChannelLayout = 0;
pub const LAYOUT_DUAL_MONO: ChannelLayout = 1;
pub const LAYOUT_DUAL_MONO_LFE: ChannelLayout = 2;
pub const LAYOUT_MONO: ChannelLayout = 3;
pub const LAYOUT_MONO_LFE: ChannelLayout = 4;
pub const LAYOUT_STEREO: ChannelLayout = 5;
pub const LAYOUT_STEREO_LFE: ChannelLayout = 6;
pub const LAYOUT_3F: ChannelLayout = 7;
pub const LAYOUT_3F_LFE: ChannelLayout = 8;
pub const LAYOUT_2F1: ChannelLayout = 9;
pub const LAYOUT_2F1_LFE: ChannelLayout = 10;
pub const LAYOUT_3F1: ChannelLayout = 11;
pub const LAYOUT_3F1_LFE: ChannelLayout = 12;
pub const LAYOUT_2F2: ChannelLayout = 13;
pub const LAYOUT_2F2_LFE: ChannelLayout = 14;
pub const LAYOUT_3F2: ChannelLayout = 15;
pub const LAYOUT_3F2_LFE: ChannelLayout = 16;
pub const LAYOUT_3F3R_LFE: ChannelLayout = 17;
pub const LAYOUT_3F4_LFE: ChannelLayout = 18;
pub const LAYOUT_MAX: ChannelLayout = 256;

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
pub type State = c_int;
pub const STATE_STARTED: State = 0;
pub const STATE_STOPPED: State = 1;
pub const STATE_DRAINED: State = 2;
pub const STATE_ERROR: State = 3;

pub const OK: i32 = 0;
pub const ERROR: i32 = -1;
pub const ERROR_INVALID_FORMAT: i32 = -2;
pub const ERROR_INVALID_PARAMETER: i32 = -3;
pub const ERROR_NOT_SUPPORTED: i32 = -4;
pub const ERROR_DEVICE_UNAVAILABLE: i32 = -5;

// These need to match cubeb_device_type
bitflags! {
    #[repr(C)]
    pub struct DeviceType : u32 {
        const UNKNOWN = 0b00;
        const INPUT = 0b01;
        const OUTPUT = 0b10;
        const ALL = 0b11;
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
    pub struct DeviceFmt: u32 {
        const S16LE = 0x0010;
        const S16BE = 0x0020;
        const S16NE = {
            #[cfg(target_endian = "little")] { DeviceFmt::S16LE }
            #[cfg(target_endian = "big")] { DeviceFmt::S16BE }
        }.bits;
        const F32LE = 0x1000;
        const F32BE = 0x2000;
        const F32NE = {
            #[cfg(target_endian = "little")] { DeviceFmt::F32LE }
            #[cfg(target_endian = "big")] { DeviceFmt::F32BE }
        }.bits;
        const S16_MASK = DeviceFmt::S16LE.bits | DeviceFmt::S16BE.bits;
        const F32_MASK = DeviceFmt::F32LE.bits | DeviceFmt::F32BE.bits;
        const ALL = DeviceFmt::S16_MASK.bits | DeviceFmt::F32_MASK.bits;
    }
}

// These need to match cubeb_device_pref
bitflags! {
    #[repr(C)]
    pub struct DevicePref : u32 {
        const MULTIMEDIA = 0x1;
        const VOICE = 0x2;
        const NOTIFICATION = 0x4;
        const ALL = 0xF;
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
    pub stream_reset_default_device: Option<unsafe extern "C" fn(stream: *mut Stream) -> i32>,
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
pub type Channel = c_int;

// These need to match cubeb_channel
pub const CHANNEL_INVALID: Channel = -1;
pub const CHANNEL_MONO: Channel = 0;
pub const CHANNEL_LEFT: Channel = 1;
pub const CHANNEL_RIGHT: Channel = 2;
pub const CHANNEL_CENTER: Channel = 3;
pub const CHANNEL_LS: Channel = 4;
pub const CHANNEL_RS: Channel = 5;
pub const CHANNEL_RLS: Channel = 6;
pub const CHANNEL_RCENTER: Channel = 7;
pub const CHANNEL_RRS: Channel = 8;
pub const CHANNEL_LFE: Channel = 9;
pub const CHANNEL_MAX: Channel = 10;

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct ChannelMap {
    pub channels: c_uint,
    pub map: [Channel; CHANNEL_MAX as usize],
}
impl ::std::default::Default for ChannelMap {
    fn default() -> Self {
        ChannelMap {
            channels: 0,
            map: [CHANNEL_INVALID,
                  CHANNEL_INVALID,
                  CHANNEL_INVALID,
                  CHANNEL_INVALID,
                  CHANNEL_INVALID,
                  CHANNEL_INVALID,
                  CHANNEL_INVALID,
                  CHANNEL_INVALID,
                  CHANNEL_INVALID,
                  CHANNEL_INVALID],
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
