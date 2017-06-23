// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

#[macro_use]
extern crate bitflags;
extern crate pulse_ffi as ffi;

#[macro_use]
mod error;
mod context;
mod mainloop_api;
mod operation;
mod proplist;
mod stream;
mod threaded_mainloop;
mod util;

pub use context::Context;
pub use error::ErrorCode;
pub use ffi::pa_buffer_attr as BufferAttr;
pub use ffi::pa_channel_map as ChannelMap;
pub use ffi::pa_cvolume as CVolume;
pub use ffi::pa_sample_spec as SampleSpec;
pub use ffi::pa_server_info as ServerInfo;
pub use ffi::pa_sink_info as SinkInfo;
pub use ffi::pa_sink_input_info as SinkInputInfo;
pub use ffi::pa_source_info as SourceInfo;
pub use ffi::pa_usec_t as USec;
pub use ffi::pa_volume_t as Volume;
pub use ffi::timeval as TimeVal;
pub use mainloop_api::MainloopApi;
pub use operation::Operation;
pub use proplist::Proplist;
use std::os::raw::{c_char, c_uint};
pub use stream::Stream;
pub use threaded_mainloop::ThreadedMainloop;

#[allow(non_camel_case_types)]
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SampleFormat {
    Invalid = ffi::PA_SAMPLE_INVALID,
    U8 = ffi::PA_SAMPLE_U8,
    Alaw = ffi::PA_SAMPLE_ALAW,
    Ulaw = ffi::PA_SAMPLE_ULAW,
    Signed16LE = ffi::PA_SAMPLE_S16LE,
    Signed16BE = ffi::PA_SAMPLE_S16BE,
    Float32LE = ffi::PA_SAMPLE_FLOAT32LE,
    Float32BE = ffi::PA_SAMPLE_FLOAT32BE,
    Signed32LE = ffi::PA_SAMPLE_S32LE,
    Signed32BE = ffi::PA_SAMPLE_S32BE,
    Signed24LE = ffi::PA_SAMPLE_S24LE,
    Signed24BE = ffi::PA_SAMPLE_S24BE,
    Signed24_32LE = ffi::PA_SAMPLE_S24_32LE,
    Signed23_32BE = ffi::PA_SAMPLE_S24_32BE,
}

impl Default for SampleFormat {
    fn default() -> Self {
        SampleFormat::Invalid
    }
}

impl Into<ffi::pa_sample_format_t> for SampleFormat {
    fn into(self) -> ffi::pa_sample_format_t {
        self as ffi::pa_sample_format_t
    }
}

#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ContextState {
    Unconnected = ffi::PA_CONTEXT_UNCONNECTED,
    Connecting = ffi::PA_CONTEXT_CONNECTING,
    Authorizing = ffi::PA_CONTEXT_AUTHORIZING,
    SettingName = ffi::PA_CONTEXT_SETTING_NAME,
    Ready = ffi::PA_CONTEXT_READY,
    Failed = ffi::PA_CONTEXT_FAILED,
    Terminated = ffi::PA_CONTEXT_TERMINATED,
}

impl ContextState {
    // This function implements the PA_CONTENT_IS_GOOD macro from pulse/def.h
    // It must match the version from PA headers.
    pub fn is_good(self) -> bool {
        match self {
            ContextState::Connecting |
            ContextState::Authorizing |
            ContextState::SettingName |
            ContextState::Ready => true,
            _ => false,
        }
    }

    pub fn try_from(x: ffi::pa_context_state_t) -> Option<Self> {
        if x >= ffi::PA_CONTEXT_UNCONNECTED && x <= ffi::PA_CONTEXT_TERMINATED {
            Some(unsafe { ::std::mem::transmute(x) })
        } else {
            None
        }
    }
}

impl Default for ContextState {
    fn default() -> Self {
        ContextState::Unconnected
    }
}

impl Into<ffi::pa_context_state_t> for ContextState {
    fn into(self) -> ffi::pa_context_state_t {
        self as ffi::pa_context_state_t
    }
}

#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum StreamState {
    Unconnected = ffi::PA_STREAM_UNCONNECTED,
    Creating = ffi::PA_STREAM_CREATING,
    Ready = ffi::PA_STREAM_READY,
    Failed = ffi::PA_STREAM_FAILED,
    Terminated = ffi::PA_STREAM_TERMINATED,
}

impl StreamState {
    // This function implements the PA_STREAM_IS_GOOD macro from pulse/def.h
    // It must match the version from PA headers.
    pub fn is_good(self) -> bool {
        match self {
            StreamState::Creating | StreamState::Ready => true,
            _ => false,
        }
    }

    pub fn try_from(x: ffi::pa_stream_state_t) -> Option<Self> {
        if x >= ffi::PA_STREAM_UNCONNECTED && x <= ffi::PA_STREAM_TERMINATED {
            Some(unsafe { ::std::mem::transmute(x) })
        } else {
            None
        }
    }
}

impl Default for StreamState {
    fn default() -> Self {
        StreamState::Unconnected
    }
}

impl Into<ffi::pa_stream_state_t> for StreamState {
    fn into(self) -> ffi::pa_stream_state_t {
        self as ffi::pa_stream_state_t
    }
}

#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum OperationState {
    Running = ffi::PA_OPERATION_RUNNING,
    Done = ffi::PA_OPERATION_DONE,
    Cancelled = ffi::PA_OPERATION_CANCELLED,
}

impl OperationState {
    pub fn try_from(x: ffi::pa_operation_state_t) -> Option<Self> {
        if x >= ffi::PA_OPERATION_RUNNING && x <= ffi::PA_OPERATION_CANCELLED {
            Some(unsafe { ::std::mem::transmute(x) })
        } else {
            None
        }
    }
}

impl Into<ffi::pa_operation_state_t> for OperationState {
    fn into(self) -> ffi::pa_operation_state_t {
        self as ffi::pa_operation_state_t
    }
}

bitflags! {
    pub flags ContextFlags: u32 {
        const CONTEXT_FLAGS_NOAUTOSPAWN = ffi::PA_CONTEXT_NOAUTOSPAWN,
        const CONTEXT_FLAGS_NOFAIL = ffi::PA_CONTEXT_NOFAIL,
    }
}

impl Into<ffi::pa_context_flags_t> for ContextFlags {
    fn into(self) -> ffi::pa_context_flags_t {
        self.bits() as ffi::pa_context_flags_t
    }
}

#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum DeviceType {
    Sink = ffi::PA_DEVICE_TYPE_SINK,
    Source = ffi::PA_DEVICE_TYPE_SOURCE,
}

impl DeviceType {
    pub fn try_from(x: ffi::pa_device_type_t) -> Option<Self> {
        if x >= ffi::PA_DEVICE_TYPE_SINK && x <= ffi::PA_DEVICE_TYPE_SOURCE {
            Some(unsafe { ::std::mem::transmute(x) })
        } else {
            None
        }
    }
}

impl Into<ffi::pa_device_type_t> for DeviceType {
    fn into(self) -> ffi::pa_device_type_t {
        self as ffi::pa_device_type_t
    }
}


#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum StreamDirection {
    NoDirection = ffi::PA_STREAM_NODIRECTION,
    Playback = ffi::PA_STREAM_PLAYBACK,
    Record = ffi::PA_STREAM_RECORD,
    StreamUpload = ffi::PA_STREAM_UPLOAD,
}

impl StreamDirection {
    pub fn try_from(x: ffi::pa_stream_direction_t) -> Option<Self> {
        if x >= ffi::PA_STREAM_NODIRECTION && x <= ffi::PA_STREAM_UPLOAD {
            Some(unsafe { ::std::mem::transmute(x) })
        } else {
            None
        }
    }
}

impl Into<ffi::pa_stream_direction_t> for StreamDirection {
    fn into(self) -> ffi::pa_stream_direction_t {
        self as ffi::pa_stream_direction_t
    }
}

bitflags! {
    pub flags StreamFlags : u32 {
        const STREAM_START_CORKED = ffi::PA_STREAM_START_CORKED,
        const STREAM_INTERPOLATE_TIMING = ffi::PA_STREAM_INTERPOLATE_TIMING,
        const STREAM_NOT_MONOTONIC = ffi::PA_STREAM_NOT_MONOTONIC,
        const STREAM_AUTO_TIMING_UPDATE = ffi::PA_STREAM_AUTO_TIMING_UPDATE,
        const STREAM_NO_REMAP_CHANNELS = ffi::PA_STREAM_NO_REMAP_CHANNELS,
        const STREAM_NO_REMIX_CHANNELS = ffi::PA_STREAM_NO_REMIX_CHANNELS,
        const STREAM_FIX_FORMAT = ffi::PA_STREAM_FIX_FORMAT,
        const STREAM_FIX_RATE = ffi::PA_STREAM_FIX_RATE,
        const STREAM_FIX_CHANNELS = ffi::PA_STREAM_FIX_CHANNELS,
        const STREAM_DONT_MOVE = ffi::PA_STREAM_DONT_MOVE,
        const STREAM_VARIABLE_RATE = ffi::PA_STREAM_VARIABLE_RATE,
        const STREAM_PEAK_DETECT = ffi::PA_STREAM_PEAK_DETECT,
        const STREAM_START_MUTED = ffi::PA_STREAM_START_MUTED,
        const STREAM_ADJUST_LATENCY = ffi::PA_STREAM_ADJUST_LATENCY,
        const STREAM_EARLY_REQUESTS = ffi::PA_STREAM_EARLY_REQUESTS,
        const STREAM_DONT_INHIBIT_AUTO_SUSPEND = ffi::PA_STREAM_DONT_INHIBIT_AUTO_SUSPEND,
        const STREAM_START_UNMUTED = ffi::PA_STREAM_START_UNMUTED,
        const STREAM_FAIL_ON_SUSPEND = ffi::PA_STREAM_FAIL_ON_SUSPEND,
        const STREAM_RELATIVE_VOLUME = ffi::PA_STREAM_RELATIVE_VOLUME,
        const STREAM_PASSTHROUGH = ffi::PA_STREAM_PASSTHROUGH,
    }
}

impl StreamFlags {
    pub fn try_from(x: ffi::pa_stream_flags_t) -> Option<Self> {
        if (x &
            !(ffi::PA_STREAM_NOFLAGS | ffi::PA_STREAM_START_CORKED | ffi::PA_STREAM_INTERPOLATE_TIMING |
              ffi::PA_STREAM_NOT_MONOTONIC | ffi::PA_STREAM_AUTO_TIMING_UPDATE |
              ffi::PA_STREAM_NO_REMAP_CHANNELS |
              ffi::PA_STREAM_NO_REMIX_CHANNELS | ffi::PA_STREAM_FIX_FORMAT | ffi::PA_STREAM_FIX_RATE |
              ffi::PA_STREAM_FIX_CHANNELS |
              ffi::PA_STREAM_DONT_MOVE | ffi::PA_STREAM_VARIABLE_RATE | ffi::PA_STREAM_PEAK_DETECT |
              ffi::PA_STREAM_START_MUTED | ffi::PA_STREAM_ADJUST_LATENCY |
              ffi::PA_STREAM_EARLY_REQUESTS |
              ffi::PA_STREAM_DONT_INHIBIT_AUTO_SUSPEND |
              ffi::PA_STREAM_START_UNMUTED | ffi::PA_STREAM_FAIL_ON_SUSPEND |
              ffi::PA_STREAM_RELATIVE_VOLUME | ffi::PA_STREAM_PASSTHROUGH)) == 0 {
            Some(unsafe { ::std::mem::transmute(x) })
        } else {
            None
        }
    }
}

impl Into<ffi::pa_stream_flags_t> for StreamFlags {
    fn into(self) -> ffi::pa_stream_flags_t {
        self.bits() as ffi::pa_stream_flags_t
    }
}

bitflags!{
    pub flags SubscriptionMask : u32 {
        const SUBSCRIPTION_MASK_SINK = ffi::PA_SUBSCRIPTION_MASK_SINK,
        const SUBSCRIPTION_MASK_SOURCE = ffi::PA_SUBSCRIPTION_MASK_SOURCE,
        const SUBSCRIPTION_MASK_SINK_INPUT = ffi::PA_SUBSCRIPTION_MASK_SINK_INPUT,
        const SUBSCRIPTION_MASK_SOURCE_OUTPUT = ffi::PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT,
        const SUBSCRIPTION_MASK_MODULE = ffi::PA_SUBSCRIPTION_MASK_MODULE,
        const SUBSCRIPTION_MASK_CLIENT = ffi::PA_SUBSCRIPTION_MASK_CLIENT,
        const SUBSCRIPTION_MASK_SAMPLE_CACHE = ffi::PA_SUBSCRIPTION_MASK_SAMPLE_CACHE,
        const SUBSCRIPTION_MASK_SERVER = ffi::PA_SUBSCRIPTION_MASK_SERVER,
        const SUBSCRIPTION_MASK_AUTOLOAD = ffi::PA_SUBSCRIPTION_MASK_AUTOLOAD,
        const SUBSCRIPTION_MASK_CARD = ffi::PA_SUBSCRIPTION_MASK_CARD,
    }
}

impl SubscriptionMask {
    pub fn try_from(x: ffi::pa_subscription_mask_t) -> Option<Self> {
        if (x & !ffi::PA_SUBSCRIPTION_MASK_ALL) == 0 {
            Some(unsafe { ::std::mem::transmute(x) })
        } else {
            None
        }
    }
}

impl Into<ffi::pa_subscription_mask_t> for SubscriptionMask {
    fn into(self) -> ffi::pa_subscription_mask_t {
        self.bits() as ffi::pa_subscription_mask_t
    }
}

#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SubscriptionEventFacility {
    Sink = ffi::PA_SUBSCRIPTION_EVENT_SINK,
    Source = ffi::PA_SUBSCRIPTION_EVENT_SOURCE,
    SinkInput = ffi::PA_SUBSCRIPTION_EVENT_SINK_INPUT,
    SourceOutput = ffi::PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT,
    Module = ffi::PA_SUBSCRIPTION_EVENT_MODULE,
    Client = ffi::PA_SUBSCRIPTION_EVENT_CLIENT,
    SampleCache = ffi::PA_SUBSCRIPTION_EVENT_SAMPLE_CACHE,
    Server = ffi::PA_SUBSCRIPTION_EVENT_SERVER,
    Autoload = ffi::PA_SUBSCRIPTION_EVENT_AUTOLOAD,
    Card = ffi::PA_SUBSCRIPTION_EVENT_CARD,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SubscriptionEventType {
    New,
    Change,
    Remove,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct SubscriptionEvent(ffi::pa_subscription_event_type_t);
impl SubscriptionEvent {
    pub fn try_from(x: ffi::pa_subscription_event_type_t) -> Option<Self> {
        if (x & !(ffi::PA_SUBSCRIPTION_EVENT_TYPE_MASK | ffi::PA_SUBSCRIPTION_EVENT_FACILITY_MASK)) == 0 {
            Some(SubscriptionEvent(x))
        } else {
            None
        }
    }

    pub fn event_facility(self) -> SubscriptionEventFacility {
        unsafe { ::std::mem::transmute(self.0 & ffi::PA_SUBSCRIPTION_EVENT_FACILITY_MASK) }
    }

    pub fn event_type(self) -> SubscriptionEventType {
        unsafe { ::std::mem::transmute(((self.0 & ffi::PA_SUBSCRIPTION_EVENT_TYPE_MASK) >> 4)) }
    }
}

#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SeekMode {
    Relative = ffi::PA_SEEK_RELATIVE,
    Absolute = ffi::PA_SEEK_ABSOLUTE,
    RelativeOnRead = ffi::PA_SEEK_RELATIVE_ON_READ,
    RelativeEnd = ffi::PA_SEEK_RELATIVE_END,
}

impl SeekMode {
    pub fn try_from(x: ffi::pa_seek_mode_t) -> Option<Self> {
        if x >= ffi::PA_SEEK_RELATIVE && x <= ffi::PA_SEEK_RELATIVE_END {
            Some(unsafe { ::std::mem::transmute(x) })
        } else {
            None
        }
    }
}

impl Into<ffi::pa_seek_mode_t> for SeekMode {
    fn into(self) -> ffi::pa_seek_mode_t {
        self as ffi::pa_seek_mode_t
    }
}

bitflags! {
    pub flags SinkFlags: u32 {
        const SINK_HW_VOLUME_CTRL = ffi::PA_SINK_HW_VOLUME_CTRL,
        const SINK_LATENCY = ffi::PA_SINK_LATENCY,
        const SINK_HARDWARE = ffi::PA_SINK_HARDWARE,
        const SINK_NETWORK = ffi::PA_SINK_NETWORK,
        const SINK_HW_MUTE_CTRL = ffi::PA_SINK_HW_MUTE_CTRL,
        const SINK_DECIBEL_VOLUME = ffi::PA_SINK_DECIBEL_VOLUME,
        const SINK_FLAT_VOLUME = ffi::PA_SINK_FLAT_VOLUME,
        const SINK_DYNAMIC_LATENCY = ffi::PA_SINK_DYNAMIC_LATENCY,
        const SINK_SET_FORMATS = ffi::PA_SINK_SET_FORMATS,
    }
}

impl SinkFlags {
    pub fn try_from(x: ffi::pa_sink_flags_t) -> Option<SinkFlags> {
        if (x &
            !(ffi::PA_SOURCE_NOFLAGS | ffi::PA_SOURCE_HW_VOLUME_CTRL | ffi::PA_SOURCE_LATENCY |
              ffi::PA_SOURCE_HARDWARE | ffi::PA_SOURCE_NETWORK | ffi::PA_SOURCE_HW_MUTE_CTRL |
              ffi::PA_SOURCE_DECIBEL_VOLUME |
              ffi::PA_SOURCE_DYNAMIC_LATENCY | ffi::PA_SOURCE_FLAT_VOLUME)) == 0 {
            Some(unsafe { ::std::mem::transmute(x) })
        } else {
            None
        }
    }
}

#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SinkState {
    InvalidState = ffi::PA_SINK_INVALID_STATE,
    Running = ffi::PA_SINK_RUNNING,
    Idle = ffi::PA_SINK_IDLE,
    Suspended = ffi::PA_SINK_SUSPENDED,
    Init = ffi::PA_SINK_INIT,
    Unlinked = ffi::PA_SINK_UNLINKED,
}

bitflags!{
    pub flags SourceFlags: u32 {
        const SOURCE_FLAGS_HW_VOLUME_CTRL = ffi::PA_SOURCE_HW_VOLUME_CTRL,
        const SOURCE_FLAGS_LATENCY = ffi::PA_SOURCE_LATENCY,
        const SOURCE_FLAGS_HARDWARE = ffi::PA_SOURCE_HARDWARE,
        const SOURCE_FLAGS_NETWORK = ffi::PA_SOURCE_NETWORK,
        const SOURCE_FLAGS_HW_MUTE_CTRL = ffi::PA_SOURCE_HW_MUTE_CTRL,
        const SOURCE_FLAGS_DECIBEL_VOLUME = ffi::PA_SOURCE_DECIBEL_VOLUME,
        const SOURCE_FLAGS_DYNAMIC_LATENCY = ffi::PA_SOURCE_DYNAMIC_LATENCY,
        const SOURCE_FLAGS_FLAT_VOLUME = ffi::PA_SOURCE_FLAT_VOLUME,
    }
}

impl Into<ffi::pa_source_flags_t> for SourceFlags {
    fn into(self) -> ffi::pa_source_flags_t {
        self.bits() as ffi::pa_source_flags_t
    }
}

#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SourceState {
    InvalidState = ffi::PA_SOURCE_INVALID_STATE,
    Running = ffi::PA_SOURCE_RUNNING,
    Idle = ffi::PA_SOURCE_IDLE,
    Suspended = ffi::PA_SOURCE_SUSPENDED,
    Init = ffi::PA_SOURCE_INIT,
    Unlinked = ffi::PA_SOURCE_UNLINKED,
}

#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PortAvailable {
    Unknown = ffi::PA_PORT_AVAILABLE_UNKNOWN,
    No = ffi::PA_PORT_AVAILABLE_NO,
    Yes = ffi::PA_PORT_AVAILABLE_YES,
}

impl PortAvailable {
    pub fn try_from(x: ffi::pa_port_available_t) -> Option<Self> {
        if x >= ffi::PA_PORT_AVAILABLE_UNKNOWN && x <= ffi::PA_PORT_AVAILABLE_YES {
            Some(unsafe { ::std::mem::transmute(x) })
        } else {
            None
        }
    }
}

impl Into<ffi::pa_port_available_t> for PortAvailable {
    fn into(self) -> ffi::pa_port_available_t {
        self as ffi::pa_port_available_t
    }
}

#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ChannelPosition {
    Invalid = ffi::PA_CHANNEL_POSITION_INVALID,
    Mono = ffi::PA_CHANNEL_POSITION_MONO,
    FrontLeft = ffi::PA_CHANNEL_POSITION_FRONT_LEFT,
    FrontRight = ffi::PA_CHANNEL_POSITION_FRONT_RIGHT,
    FrontCenter = ffi::PA_CHANNEL_POSITION_FRONT_CENTER,
    RearCenter = ffi::PA_CHANNEL_POSITION_REAR_CENTER,
    RearLeft = ffi::PA_CHANNEL_POSITION_REAR_LEFT,
    RearRight = ffi::PA_CHANNEL_POSITION_REAR_RIGHT,
    LowFreqEffects = ffi::PA_CHANNEL_POSITION_LFE,
    FrontLeftOfCenter = ffi::PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER,
    FrontRightOfCenter = ffi::PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER,
    SideLeft = ffi::PA_CHANNEL_POSITION_SIDE_LEFT,
    SideRight = ffi::PA_CHANNEL_POSITION_SIDE_RIGHT,
    Aux0 = ffi::PA_CHANNEL_POSITION_AUX0,
    Aux1 = ffi::PA_CHANNEL_POSITION_AUX1,
    Aux2 = ffi::PA_CHANNEL_POSITION_AUX2,
    Aux3 = ffi::PA_CHANNEL_POSITION_AUX3,
    Aux4 = ffi::PA_CHANNEL_POSITION_AUX4,
    Aux5 = ffi::PA_CHANNEL_POSITION_AUX5,
    Aux6 = ffi::PA_CHANNEL_POSITION_AUX6,
    Aux7 = ffi::PA_CHANNEL_POSITION_AUX7,
    Aux8 = ffi::PA_CHANNEL_POSITION_AUX8,
    Aux9 = ffi::PA_CHANNEL_POSITION_AUX9,
    Aux10 = ffi::PA_CHANNEL_POSITION_AUX10,
    Aux11 = ffi::PA_CHANNEL_POSITION_AUX11,
    Aux12 = ffi::PA_CHANNEL_POSITION_AUX12,
    Aux13 = ffi::PA_CHANNEL_POSITION_AUX13,
    Aux14 = ffi::PA_CHANNEL_POSITION_AUX14,
    Aux15 = ffi::PA_CHANNEL_POSITION_AUX15,
    Aux16 = ffi::PA_CHANNEL_POSITION_AUX16,
    Aux17 = ffi::PA_CHANNEL_POSITION_AUX17,
    Aux18 = ffi::PA_CHANNEL_POSITION_AUX18,
    Aux19 = ffi::PA_CHANNEL_POSITION_AUX19,
    Aux20 = ffi::PA_CHANNEL_POSITION_AUX20,
    Aux21 = ffi::PA_CHANNEL_POSITION_AUX21,
    Aux22 = ffi::PA_CHANNEL_POSITION_AUX22,
    Aux23 = ffi::PA_CHANNEL_POSITION_AUX23,
    Aux24 = ffi::PA_CHANNEL_POSITION_AUX24,
    Aux25 = ffi::PA_CHANNEL_POSITION_AUX25,
    Aux26 = ffi::PA_CHANNEL_POSITION_AUX26,
    Aux27 = ffi::PA_CHANNEL_POSITION_AUX27,
    Aux28 = ffi::PA_CHANNEL_POSITION_AUX28,
    Aux29 = ffi::PA_CHANNEL_POSITION_AUX29,
    Aux30 = ffi::PA_CHANNEL_POSITION_AUX30,
    Aux31 = ffi::PA_CHANNEL_POSITION_AUX31,
    TopCenter = ffi::PA_CHANNEL_POSITION_TOP_CENTER,
    TopFrontLeft = ffi::PA_CHANNEL_POSITION_TOP_FRONT_LEFT,
    TopFrontRight = ffi::PA_CHANNEL_POSITION_TOP_FRONT_RIGHT,
    TopFrontCenter = ffi::PA_CHANNEL_POSITION_TOP_FRONT_CENTER,
    TopRearLeft = ffi::PA_CHANNEL_POSITION_TOP_REAR_LEFT,
    TopRearRight = ffi::PA_CHANNEL_POSITION_TOP_REAR_RIGHT,
    TopRearCenter = ffi::PA_CHANNEL_POSITION_TOP_REAR_CENTER,
}

impl ChannelPosition {
    pub fn try_from(x: ffi::pa_channel_position_t) -> Option<Self> {
        if x >= ffi::PA_CHANNEL_POSITION_INVALID && x < ffi::PA_CHANNEL_POSITION_MAX {
            Some(unsafe { ::std::mem::transmute(x) })
        } else {
            None
        }
    }
}

impl Default for ChannelPosition {
    fn default() -> Self {
        ChannelPosition::Invalid
    }
}

impl Into<ffi::pa_channel_position_t> for ChannelPosition {
    fn into(self) -> ffi::pa_channel_position_t {
        self as ffi::pa_channel_position_t
    }
}
pub type Result<T> = ::std::result::Result<T, error::ErrorCode>;

pub trait CVolumeExt {
    fn set(&mut self, channels: c_uint, v: Volume);
    fn set_balance(&mut self, map: &ChannelMap, new_balance: f32);
}

impl CVolumeExt for CVolume {
    fn set(&mut self, channels: c_uint, v: Volume) {
        unsafe {
            ffi::pa_cvolume_set(self, channels, v);
        }
    }

    fn set_balance(&mut self, map: &ChannelMap, new_balance: f32) {
        unsafe {
            ffi::pa_cvolume_set_balance(self, map, new_balance);
        }
    }
}

pub trait ChannelMapExt {
    fn init() -> ChannelMap;
    fn can_balance(&self) -> bool;
}

impl ChannelMapExt for ChannelMap {
    fn init() -> ChannelMap {
        let mut cm = ChannelMap::default();
        unsafe {
            ffi::pa_channel_map_init(&mut cm);
        }
        cm
    }
    fn can_balance(&self) -> bool {
        unsafe { ffi::pa_channel_map_can_balance(self) > 0 }
    }
}

pub trait ProplistExt {
    fn proplist(&self) -> Proplist;
}

impl ProplistExt for SinkInfo {
    fn proplist(&self) -> Proplist {
        unsafe { proplist::from_raw_ptr(self.proplist) }
    }
}

impl ProplistExt for SourceInfo {
    fn proplist(&self) -> Proplist {
        unsafe { proplist::from_raw_ptr(self.proplist) }
    }
}

pub trait SampleSpecExt {
    fn frame_size(&self) -> usize;
}

impl SampleSpecExt for SampleSpec {
    fn frame_size(&self) -> usize {
        unsafe { ffi::pa_frame_size(self) }
    }
}

pub trait USecExt {
    fn to_bytes(self, spec: &SampleSpec) -> usize;
}

impl USecExt for USec {
    fn to_bytes(self, spec: &SampleSpec) -> usize {
        unsafe { ffi::pa_usec_to_bytes(self, spec) }
    }
}

pub fn library_version() -> *const c_char {
    unsafe { ffi::pa_get_library_version() }
}

pub fn sw_volume_from_linear(vol: f64) -> Volume {
    unsafe { ffi::pa_sw_volume_from_linear(vol) }
}

pub fn rtclock_now() -> USec {
    unsafe { ffi::pa_rtclock_now() }
}
