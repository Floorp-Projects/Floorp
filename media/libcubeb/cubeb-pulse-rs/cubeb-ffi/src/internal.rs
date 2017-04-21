use libc::c_void;
use *;

#[repr(C)]
#[derive(Clone,Copy,Debug)]
pub struct LayoutMap {
    pub name: *const i8,
    pub channels: u32,
    pub layout: ChannelLayout
}

#[repr(C)]
pub struct Ops {
    pub init: Option<unsafe extern "C" fn(context: *mut *mut Context, context_name: *const i8) -> i32>,
    pub get_backend_id: Option<unsafe extern "C" fn(context: *mut Context) -> *const i8>,
    pub get_max_channel_count: Option<unsafe extern "C" fn(context: *mut Context, max_channels: *mut u32) -> i32>,
    pub get_min_latency:  Option<unsafe extern "C" fn(context: *mut Context, params: StreamParams, latency_ms: *mut u32) -> i32>,
    pub get_preferred_sample_rate:  Option<unsafe extern "C" fn(context: *mut Context, rate: *mut u32) -> i32>,
    pub get_preferred_channel_layout:  Option<unsafe extern "C" fn(context: *mut Context, layout: *mut ChannelLayout) -> i32>,
    pub enumerate_devices:  Option<unsafe extern "C" fn(context: *mut Context, devtype: DeviceType, collection: *mut *mut DeviceCollection) -> i32>,
    pub destroy:  Option<unsafe extern "C" fn(context: *mut Context)>,
    pub stream_init: Option<unsafe extern "C" fn(context: *mut Context, stream: *mut *mut Stream, stream_name: *const i8, input_device: DeviceId, input_stream_params: *mut StreamParams, output_device: DeviceId, output_stream_params: *mut StreamParams, latency: u32, data_callback: DataCallback, state_callback: StateCallback, user_ptr: *mut c_void) -> i32>,
    pub stream_destroy: Option<unsafe extern "C" fn(stream: *mut Stream)>,
    pub stream_start: Option<unsafe extern "C" fn(stream: *mut Stream) -> i32>,
    pub stream_stop: Option<unsafe extern "C" fn(stream: *mut Stream) -> i32>,
    pub stream_get_position: Option<unsafe extern "C" fn(stream: *mut Stream, position: *mut u64) -> i32>,
    pub stream_get_latency: Option<unsafe extern "C" fn(stream: *mut Stream, latency: *mut u32) -> i32>,
    pub stream_set_volume: Option<unsafe extern "C" fn(stream: *mut Stream, volumes: f32) -> i32>,
    pub stream_set_panning: Option<unsafe extern "C" fn(stream: *mut Stream, panning: f32)-> i32>,
    pub stream_get_current_device: Option<unsafe extern "C" fn(stream: *mut Stream, device: *mut *const Device) -> i32>,
    pub stream_device_destroy: Option<unsafe extern "C" fn(stream: *mut Stream, device: *mut Device) -> i32>,
    pub stream_register_device_changed_callback: Option<unsafe extern "C" fn(stream: *mut Stream, device_changed_callback: DeviceChangedCallback) -> i32>,
    pub register_device_collection_changed: Option<unsafe extern "C" fn(context: *mut Context, devtype: DeviceType, callback: DeviceCollectionChangedCallback, user_ptr: *mut c_void) -> i32>
}
