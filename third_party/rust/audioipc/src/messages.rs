// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

use crate::PlatformHandle;
use crate::PlatformHandleType;
#[cfg(target_os = "linux")]
use audio_thread_priority::RtPriorityThreadInfo;
use cubeb::{self, ffi};
use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int, c_uint};
use std::ptr;

#[derive(Debug, Serialize, Deserialize)]
pub struct Device {
    pub output_name: Option<Vec<u8>>,
    pub input_name: Option<Vec<u8>>,
}

impl<'a> From<&'a cubeb::DeviceRef> for Device {
    fn from(info: &'a cubeb::DeviceRef) -> Self {
        Self {
            output_name: info.output_name_bytes().map(|s| s.to_vec()),
            input_name: info.input_name_bytes().map(|s| s.to_vec()),
        }
    }
}

impl From<ffi::cubeb_device> for Device {
    fn from(info: ffi::cubeb_device) -> Self {
        Self {
            output_name: dup_str(info.output_name),
            input_name: dup_str(info.input_name),
        }
    }
}

impl From<Device> for ffi::cubeb_device {
    fn from(info: Device) -> Self {
        Self {
            output_name: opt_str(info.output_name),
            input_name: opt_str(info.input_name),
        }
    }
}

#[derive(Debug, Serialize, Deserialize)]
pub struct DeviceInfo {
    pub devid: usize,
    pub device_id: Option<Vec<u8>>,
    pub friendly_name: Option<Vec<u8>>,
    pub group_id: Option<Vec<u8>>,
    pub vendor_name: Option<Vec<u8>>,

    pub device_type: ffi::cubeb_device_type,
    pub state: ffi::cubeb_device_state,
    pub preferred: ffi::cubeb_device_pref,

    pub format: ffi::cubeb_device_fmt,
    pub default_format: ffi::cubeb_device_fmt,
    pub max_channels: u32,
    pub default_rate: u32,
    pub max_rate: u32,
    pub min_rate: u32,

    pub latency_lo: u32,
    pub latency_hi: u32,
}

impl<'a> From<&'a cubeb::DeviceInfoRef> for DeviceInfo {
    fn from(info: &'a cubeb::DeviceInfoRef) -> Self {
        let info = unsafe { &*info.as_ptr() };
        DeviceInfo {
            devid: info.devid as _,
            device_id: dup_str(info.device_id),
            friendly_name: dup_str(info.friendly_name),
            group_id: dup_str(info.group_id),
            vendor_name: dup_str(info.vendor_name),

            device_type: info.device_type,
            state: info.state,
            preferred: info.preferred,

            format: info.format,
            default_format: info.default_format,
            max_channels: info.max_channels,
            default_rate: info.default_rate,
            max_rate: info.max_rate,
            min_rate: info.min_rate,

            latency_lo: info.latency_lo,
            latency_hi: info.latency_hi,
        }
    }
}

impl From<DeviceInfo> for ffi::cubeb_device_info {
    fn from(info: DeviceInfo) -> Self {
        ffi::cubeb_device_info {
            devid: info.devid as _,
            device_id: opt_str(info.device_id),
            friendly_name: opt_str(info.friendly_name),
            group_id: opt_str(info.group_id),
            vendor_name: opt_str(info.vendor_name),

            device_type: info.device_type,
            state: info.state,
            preferred: info.preferred,

            format: info.format,
            default_format: info.default_format,
            max_channels: info.max_channels,
            default_rate: info.default_rate,
            max_rate: info.max_rate,
            min_rate: info.min_rate,

            latency_lo: info.latency_lo,
            latency_hi: info.latency_hi,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Serialize)]
pub struct StreamParams {
    pub format: ffi::cubeb_sample_format,
    pub rate: c_uint,
    pub channels: c_uint,
    pub layout: ffi::cubeb_channel_layout,
    pub prefs: ffi::cubeb_stream_prefs,
}

impl<'a> From<&'a cubeb::StreamParamsRef> for StreamParams {
    fn from(x: &cubeb::StreamParamsRef) -> StreamParams {
        unsafe { *(x.as_ptr() as *mut StreamParams) }
    }
}

#[derive(Debug, Serialize, Deserialize)]
pub struct StreamCreateParams {
    pub input_stream_params: Option<StreamParams>,
    pub output_stream_params: Option<StreamParams>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct StreamInitParams {
    pub stream_name: Option<Vec<u8>>,
    pub input_device: usize,
    pub input_stream_params: Option<StreamParams>,
    pub output_device: usize,
    pub output_stream_params: Option<StreamParams>,
    pub latency_frames: u32,
}

fn dup_str(s: *const c_char) -> Option<Vec<u8>> {
    if s.is_null() {
        None
    } else {
        let vec: Vec<u8> = unsafe { CStr::from_ptr(s) }.to_bytes().to_vec();
        Some(vec)
    }
}

fn opt_str(v: Option<Vec<u8>>) -> *mut c_char {
    match v {
        Some(v) => match CString::new(v) {
            Ok(s) => s.into_raw(),
            Err(_) => {
                debug!("Failed to convert bytes to CString");
                ptr::null_mut()
            }
        },
        None => ptr::null_mut(),
    }
}

#[derive(Debug, Serialize, Deserialize)]
pub struct StreamCreate {
    pub token: usize,
    pub platform_handle: SerializableHandle,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct RegisterDeviceCollectionChanged {
    pub platform_handle: SerializableHandle,
}

// Client -> Server messages.
// TODO: Callbacks should be different messages types so
// ServerConn::process_msg doesn't have a catch-all case.
#[derive(Debug, Serialize, Deserialize)]
pub enum ServerMessage {
    ClientConnect(u32),
    ClientDisconnect,

    ContextGetBackendId,
    ContextGetMaxChannelCount,
    ContextGetMinLatency(StreamParams),
    ContextGetPreferredSampleRate,
    ContextGetDeviceEnumeration(ffi::cubeb_device_type),
    ContextSetupDeviceCollectionCallback,
    ContextRegisterDeviceCollectionChanged(ffi::cubeb_device_type, bool),

    StreamCreate(StreamCreateParams),
    StreamInit(usize, StreamInitParams),
    StreamDestroy(usize),

    StreamStart(usize),
    StreamStop(usize),
    StreamGetPosition(usize),
    StreamGetLatency(usize),
    StreamGetInputLatency(usize),
    StreamSetVolume(usize, f32),
    StreamSetName(usize, CString),
    StreamGetCurrentDevice(usize),
    StreamRegisterDeviceChangeCallback(usize, bool),

    #[cfg(target_os = "linux")]
    PromoteThreadToRealTime([u8; std::mem::size_of::<RtPriorityThreadInfo>()]),
}

// Server -> Client messages.
// TODO: Streams need id.
#[derive(Debug, Serialize, Deserialize)]
pub enum ClientMessage {
    ClientConnected,
    ClientDisconnected,

    ContextBackendId(String),
    ContextMaxChannelCount(u32),
    ContextMinLatency(u32),
    ContextPreferredSampleRate(u32),
    ContextEnumeratedDevices(Vec<DeviceInfo>),
    ContextSetupDeviceCollectionCallback(RegisterDeviceCollectionChanged),
    ContextRegisteredDeviceCollectionChanged,

    StreamCreated(StreamCreate),
    StreamInitialized,
    StreamDestroyed,

    StreamStarted,
    StreamStopped,
    StreamPosition(u64),
    StreamLatency(u32),
    StreamInputLatency(u32),
    StreamVolumeSet,
    StreamNameSet,
    StreamCurrentDevice(Device),
    StreamRegisterDeviceChangeCallback,

    #[cfg(target_os = "linux")]
    ThreadPromoted,

    Error(c_int),
}

#[derive(Debug, Deserialize, Serialize)]
pub enum CallbackReq {
    Data {
        nframes: isize,
        input_frame_size: usize,
        output_frame_size: usize,
    },
    State(ffi::cubeb_state),
    DeviceChange,
    SharedMem(SerializableHandle, usize),
}

#[derive(Debug, Deserialize, Serialize)]
pub enum CallbackResp {
    Data(isize),
    State,
    DeviceChange,
    SharedMem,
}

#[derive(Debug, Deserialize, Serialize)]
pub enum DeviceCollectionReq {
    DeviceChange(ffi::cubeb_device_type),
}

#[derive(Debug, Deserialize, Serialize)]
pub enum DeviceCollectionResp {
    DeviceChange,
}

// Represents a handle in various transitional states during serialization and remoting.
// The process of serializing and remoting handles and the ownership during various states differs
// between Windows and Unix.  SerializableHandle changes during IPC is as follows:
//
// 1. The initial state `Owned`, with a valid `target_pid`.
// 2. Ownership is transferred out for processing during IPC send, becoming `Empty` temporarily.
//    See `AssocRawPlatformHandle::take_handle_for_send`.
// 3. Message containing `SerializableHandleValue` is serialized and sent via IPC.
//    - Windows: DuplicateHandle transfers the handle to the remote process.
//               This produces a new value in the local process representing the remote handle.
//               This value must be sent to the remote, so is recorded as `SerializableValue`.
//    - Unix: Handle value (and ownership) is encoded into cmsg buffer via `cmsg::builder`.
//            The handle is converted to a `SerializableValue` for convenience, but is otherwise unused.
// 4. Message received and deserialized in target process.
//    - Windows: Deserialization converts the `SerializableValue` into `Owned`, ready for use.
//    - Unix: Handle (with a new value in the target process) is received out-of-band via `recvmsg`
//            and converted to `Owned` via `AssocRawPlatformHandle::set_owned_handle`.
#[derive(Debug)]
pub enum SerializableHandle {
    // Owned handle, with optional target_pid on sending side.
    Owned(PlatformHandle, Option<u32>),
    // Transitional IPC states.
    SerializableValue(PlatformHandleType),
    Empty,
}

unsafe impl Send for SerializableHandle {}

impl SerializableHandle {
    pub fn new(handle: PlatformHandle, target_pid: u32) -> SerializableHandle {
        SerializableHandle::Owned(handle, Some(target_pid))
    }

    pub fn take_handle(&mut self) -> PlatformHandle {
        match std::mem::replace(self, SerializableHandle::Empty) {
            SerializableHandle::Owned(handle, target_pid) => {
                assert!(target_pid.is_none());
                handle
            }
            _ => panic!("take_handle called in invalid state"),
        }
    }

    unsafe fn take_handle_for_send(&mut self) -> (PlatformHandleType, u32) {
        match std::mem::replace(self, SerializableHandle::Empty) {
            SerializableHandle::Owned(handle, target_pid) => (
                handle.into_raw(),
                target_pid.expect("need valid target_pid"),
            ),
            _ => panic!("take_handle_with_target called in invalid state"),
        }
    }

    fn new_owned(handle: PlatformHandleType) -> SerializableHandle {
        SerializableHandle::Owned(PlatformHandle::new(handle), None)
    }

    fn new_serializable_value(handle: PlatformHandleType) -> SerializableHandle {
        SerializableHandle::SerializableValue(handle)
    }

    fn get_serializable_value(&self) -> PlatformHandleType {
        match *self {
            SerializableHandle::SerializableValue(handle) => handle,
            _ => panic!("get_remote_handle called in invalid state"),
        }
    }
}

// Raw handle values are serialized as i64.  Additional handling external to (de)serialization is required during IPC
// send/receive to convert these raw values into valid handles.
impl serde::Serialize for SerializableHandle {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        let handle = self.get_serializable_value();
        serializer.serialize_i64(handle as i64)
    }
}

impl<'de> serde::Deserialize<'de> for SerializableHandle {
    fn deserialize<D>(deserializer: D) -> Result<SerializableHandle, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        deserializer.deserialize_i64(SerializableHandleVisitor)
    }
}

struct SerializableHandleVisitor;
impl<'de> serde::de::Visitor<'de> for SerializableHandleVisitor {
    type Value = SerializableHandle;

    fn expecting(&self, formatter: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        formatter.write_str("an integer between -2^63 and 2^63")
    }

    fn visit_i64<E>(self, value: i64) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        let value = value as PlatformHandleType;
        Ok(if cfg!(windows) {
            SerializableHandle::new_owned(value)
        } else {
            // On Unix, SerializableHandle becomes owned once `set_owned_handle` is called
            // with the new local handle value during `recvmsg`.
            SerializableHandle::new_serializable_value(value)
        })
    }
}

pub trait AssocRawPlatformHandle {
    // Transfer ownership of handle (if any) to caller.
    // The caller may then replace the handle using the platform-specific methods below.
    fn take_handle_for_send(&mut self) -> Option<(PlatformHandleType, u32)> {
        None
    }

    // Update the item's handle to reflect the new remote handle value.
    // Called on the sending side prior to serialization.
    fn set_remote_handle_value<F>(&mut self, f: F)
    where
        F: FnOnce() -> Option<PlatformHandleType>,
    {
        assert!(f().is_none());
    }

    // Update the item's handle with the received value, making it a valid owned handle.
    // Called on the receiving side after deserialization.
    #[cfg(unix)]
    fn set_owned_handle<F>(&mut self, f: F)
    where
        F: FnOnce() -> Option<PlatformHandleType>,
    {
        assert!(f().is_none());
    }
}

impl AssocRawPlatformHandle for ServerMessage {}

impl AssocRawPlatformHandle for ClientMessage {
    fn take_handle_for_send(&mut self) -> Option<(PlatformHandleType, u32)> {
        unsafe {
            match *self {
                ClientMessage::StreamCreated(ref mut data) => {
                    Some(data.platform_handle.take_handle_for_send())
                }
                ClientMessage::ContextSetupDeviceCollectionCallback(ref mut data) => {
                    Some(data.platform_handle.take_handle_for_send())
                }
                _ => None,
            }
        }
    }

    fn set_remote_handle_value<F>(&mut self, f: F)
    where
        F: FnOnce() -> Option<PlatformHandleType>,
    {
        match *self {
            ClientMessage::StreamCreated(ref mut data) => {
                let handle =
                    f().expect("platform_handle must be available when processing StreamCreated");
                data.platform_handle = SerializableHandle::new_serializable_value(handle);
            }
            ClientMessage::ContextSetupDeviceCollectionCallback(ref mut data) => {
                let handle = f().expect("platform_handle must be available when processing ContextSetupDeviceCollectionCallback");
                data.platform_handle = SerializableHandle::new_serializable_value(handle);
            }
            _ => {}
        }
    }

    #[cfg(unix)]
    fn set_owned_handle<F>(&mut self, f: F)
    where
        F: FnOnce() -> Option<PlatformHandleType>,
    {
        match *self {
            ClientMessage::StreamCreated(ref mut data) => {
                let handle =
                    f().expect("platform_handle must be available when processing StreamCreated");
                data.platform_handle = SerializableHandle::new_owned(handle);
            }
            ClientMessage::ContextSetupDeviceCollectionCallback(ref mut data) => {
                let handle = f().expect("platform_handle must be available when processing ContextSetupDeviceCollectionCallback");
                data.platform_handle = SerializableHandle::new_owned(handle);
            }
            _ => {}
        }
    }
}

impl AssocRawPlatformHandle for DeviceCollectionReq {}
impl AssocRawPlatformHandle for DeviceCollectionResp {}

impl AssocRawPlatformHandle for CallbackReq {
    fn take_handle_for_send(&mut self) -> Option<(PlatformHandleType, u32)> {
        unsafe {
            if let CallbackReq::SharedMem(ref mut data, _) = *self {
                Some(data.take_handle_for_send())
            } else {
                None
            }
        }
    }

    fn set_remote_handle_value<F>(&mut self, f: F)
    where
        F: FnOnce() -> Option<PlatformHandleType>,
    {
        if let CallbackReq::SharedMem(ref mut data, _) = *self {
            let handle = f().expect("platform_handle must be available when processing SharedMem");
            *data = SerializableHandle::new_serializable_value(handle);
        }
    }

    #[cfg(unix)]
    fn set_owned_handle<F>(&mut self, f: F)
    where
        F: FnOnce() -> Option<PlatformHandleType>,
    {
        if let CallbackReq::SharedMem(ref mut data, _) = *self {
            let handle = f().expect("platform_handle must be available when processing SharedMem");
            *data = SerializableHandle::new_owned(handle);
        }
    }
}

impl AssocRawPlatformHandle for CallbackResp {}

#[cfg(test)]
mod test {
    use super::StreamParams;
    use cubeb::ffi;
    use std::mem;

    #[test]
    fn stream_params_size_check() {
        assert_eq!(
            mem::size_of::<StreamParams>(),
            mem::size_of::<ffi::cubeb_stream_params>()
        )
    }

    #[test]
    fn stream_params_from() {
        let raw = ffi::cubeb_stream_params {
            format: ffi::CUBEB_SAMPLE_FLOAT32BE,
            rate: 96_000,
            channels: 32,
            layout: ffi::CUBEB_LAYOUT_3F1_LFE,
            prefs: ffi::CUBEB_STREAM_PREF_LOOPBACK,
        };
        let wrapped = ::cubeb::StreamParams::from(raw);
        let params = StreamParams::from(wrapped.as_ref());
        assert_eq!(params.format, raw.format);
        assert_eq!(params.rate, raw.rate);
        assert_eq!(params.channels, raw.channels);
        assert_eq!(params.layout, raw.layout);
        assert_eq!(params.prefs, raw.prefs);
    }
}
