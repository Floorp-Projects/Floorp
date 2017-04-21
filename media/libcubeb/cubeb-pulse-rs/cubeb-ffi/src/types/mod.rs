use libc::c_void;

mod device_pref;
mod device_fmt;
mod device_type;

pub use self::device_pref::*;
pub use self::device_fmt::*;
pub use self::device_type::*;

/// Opaque handle to cubeb context.
pub enum Context {}

/// Opaque handle to cubeb stream.
pub enum Stream {}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum SampleFormat {
    Signed16LE = 0,
    Signed16BE = 1,
    Float32LE = 2,
    Float32BE = 3,
}

#[cfg(target_endian = "little")]
pub const SAMPLE_S16NE: SampleFormat = SampleFormat::Signed16LE;
#[cfg(target_endian = "little")]
pub const SAMPLE_FLOAT32NE: SampleFormat = SampleFormat::Float32LE;
#[cfg(target_endian = "big")]
pub const SAMPLE_S16NE: SampleFormat = SampleFormat::Signed16BE;
#[cfg(target_endian = "big")]
pub const SAMPLE_FLOAT32NE: SampleFormat = SampleFormat::Float32BE;

pub type DeviceId = *const c_void;

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum ChannelLayout {
    Undefined = 0,
    DualMono = 1,
    DualMonoLfe = 2,
    Mono = 3,
    MonoLfe = 4,
    Stereo = 5,
    StereoLfe = 6,
    Layout3F = 7,
    Layout3FLfe = 8,
    Layout2F1 = 9,
    Layout2F1Lfe = 10,
    Layout3F1 = 11,
    Layout3F1Lfe = 12,
    Layout2F2 = 13,
    Layout2F2Lfe = 14,
    Layout3F2 = 15,
    Layout3F2Lfe = 16,
    Layout3F3RLfe = 17,
    Layout3F4Lfe = 18,
    Max = 19,
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct StreamParams {
    pub format: SampleFormat,
    pub rate: u32,
    pub channels: u32,
    pub layout: ChannelLayout,
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct Device {
    pub output_name: *mut i8,
    pub input_name: *mut i8,
}

impl Default for Device {
    fn default() -> Self {
        Device {
            output_name: 0 as *mut _,
            input_name: 0 as *mut _,
        }
    }
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum State {
    Started = 0,
    Stopped = 1,
    Drained = 2,
    Error = 3,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum DeviceState {
    Disabled = 0,
    Unplugged = 1,
    Enabled = 2,
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct DeviceInfo {
    pub devid: DeviceId,
    pub device_id: *const i8,
    pub friendly_name: *const i8,
    pub group_id: *const i8,
    pub vendor_name: *const i8,
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
#[derive(Copy, Clone, Debug)]
pub struct DeviceCollection {
    /// Device count in collection.
    pub count: u32,
    /// Array of pointers to device info.
    pub device: [*const DeviceInfo; 0],
}

pub type DataCallback = Option<unsafe extern "C" fn(stream: *mut Stream, user_ptr: *mut c_void, input_buffer: *const c_void, output_buffer: *mut c_void, nframes: i64) -> i64>;
pub type StateCallback = Option<unsafe extern "C" fn(stream: *mut Stream, user_ptr: *mut c_void, state: State)>;
pub type DeviceChangedCallback = Option<unsafe extern "C" fn(user_ptr: *mut c_void)>;
pub type DeviceCollectionChangedCallback = Option<unsafe extern "C" fn(context: *mut Context, user_ptr: *mut c_void)>;
pub type LogCallback = Option<unsafe extern "C" fn(fmt: *const i8, ...)>;

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
