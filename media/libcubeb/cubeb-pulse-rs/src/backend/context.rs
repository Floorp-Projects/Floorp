// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use backend::*;
use capi::PULSE_OPS;
use cubeb;
use pulse::{self, ProplistExt};
use pulse_ffi::*;
use semver;
use std::default::Default;
use std::ffi::{CStr, CString};
use std::mem;
use std::os::raw::{c_char, c_void};
use std::ptr;
use std::cell::RefCell;

fn pa_channel_to_cubeb_channel(channel: pulse::ChannelPosition) -> cubeb::Channel {
    use pulse::ChannelPosition;
    assert_ne!(channel, ChannelPosition::Invalid);
    match channel {
        ChannelPosition::Mono => cubeb::CHANNEL_MONO,
        ChannelPosition::FrontLeft => cubeb::CHANNEL_LEFT,
        ChannelPosition::FrontRight => cubeb::CHANNEL_RIGHT,
        ChannelPosition::FrontCenter => cubeb::CHANNEL_CENTER,
        ChannelPosition::SideLeft => cubeb::CHANNEL_LS,
        ChannelPosition::SideRight => cubeb::CHANNEL_RS,
        ChannelPosition::RearLeft => cubeb::CHANNEL_RLS,
        ChannelPosition::RearCenter => cubeb::CHANNEL_RCENTER,
        ChannelPosition::RearRight => cubeb::CHANNEL_RRS,
        ChannelPosition::LowFreqEffects => cubeb::CHANNEL_LFE,
        _ => cubeb::CHANNEL_INVALID,
    }
}

fn channel_map_to_layout(cm: &pulse::ChannelMap) -> cubeb::ChannelLayout {
    use pulse::ChannelPosition;
    let mut cubeb_map: cubeb::ChannelMap = Default::default();
    cubeb_map.channels = cm.channels as u32;
    for i in 0usize..cm.channels as usize {
        cubeb_map.map[i] = pa_channel_to_cubeb_channel(ChannelPosition::try_from(cm.map[i])
                                                           .unwrap_or(ChannelPosition::Invalid));
    }
    unsafe { cubeb::cubeb_channel_map_to_layout(&cubeb_map) }
}

#[derive(Debug)]
pub struct DefaultInfo {
    pub sample_spec: pulse::SampleSpec,
    pub channel_map: pulse::ChannelMap,
    pub flags: pulse::SinkFlags,
}

#[derive(Debug)]
pub struct Context {
    pub ops: *const cubeb::Ops,
    pub mainloop: pulse::ThreadedMainloop,
    pub context: Option<pulse::Context>,
    pub default_sink_info: Option<DefaultInfo>,
    pub context_name: Option<CString>,
    pub collection_changed_callback: cubeb::DeviceCollectionChangedCallback,
    pub collection_changed_user_ptr: *mut c_void,
    pub error: bool,
    pub version_2_0_0: bool,
    pub version_0_9_8: bool,
    #[cfg(feature = "pulse-dlopen")]
    pub libpulse: LibLoader,
    devids: RefCell<Intern>,
}

impl Drop for Context {
    fn drop(&mut self) {
        self.destroy();
    }
}

impl Context {
    #[cfg(feature = "pulse-dlopen")]
    fn _new(name: Option<CString>) -> Result<Box<Self>> {
        let libpulse = unsafe { open() };
        if libpulse.is_none() {
            return Err(cubeb::ERROR);
        }

        let ctx = Box::new(Context {
                               ops: &PULSE_OPS,
                               libpulse: libpulse.unwrap(),
                               mainloop: pulse::ThreadedMainloop::new(),
                               context: None,
                               default_sink_info: None,
                               context_name: name,
                               collection_changed_callback: None,
                               collection_changed_user_ptr: ptr::null_mut(),
                               error: true,
                               version_0_9_8: false,
                               version_2_0_0: false,
                               devids: RefCell::new(Intern::new()),
                           });

        Ok(ctx)
    }

    #[cfg(not(feature = "pulse-dlopen"))]
    fn _new(name: Option<CString>) -> Result<Box<Self>> {
        Ok(Box::new(Context {
                        ops: &PULSE_OPS,
                        mainloop: pulse::ThreadedMainloop::new(),
                        context: None,
                        default_sink_info: None,
                        context_name: name,
                        collection_changed_callback: None,
                        collection_changed_user_ptr: ptr::null_mut(),
                        error: true,
                        version_0_9_8: false,
                        version_2_0_0: false,
                        devids: RefCell::new(Intern::new()),
                    }))
    }

    pub fn new(name: *const c_char) -> Result<Box<Self>> {
        fn server_info_cb(context: &pulse::Context, info: &pulse::ServerInfo, u: *mut c_void) {
            fn sink_info_cb(_: &pulse::Context, i: *const pulse::SinkInfo, eol: i32, u: *mut c_void) {
                let ctx = unsafe { &mut *(u as *mut Context) };
                if eol == 0 {
                    let info = unsafe { &*i };
                    let flags = pulse::SinkFlags::from_bits_truncate(info.flags);
                    ctx.default_sink_info = Some(DefaultInfo {
                                                     sample_spec: info.sample_spec,
                                                     channel_map: info.channel_map,
                                                     flags: flags,
                                                 });
                }
                ctx.mainloop.signal();
            }

            let _ = context.get_sink_info_by_name(try_cstr_from(info.default_sink_name),
                                                  sink_info_cb,
                                                  u);
        }

        let name = super::try_cstr_from(name).map(|s| s.to_owned());
        let mut ctx = try!(Context::_new(name));

        if ctx.mainloop.start().is_err() {
            ctx.destroy();
            return Err(cubeb::ERROR);
        }

        if ctx.context_init() != cubeb::OK {
            ctx.destroy();
            return Err(cubeb::ERROR);
        }

        ctx.mainloop.lock();
        /* server_info_callback performs a second async query,
         * which is responsible for initializing default_sink_info
         * and signalling the mainloop to end the wait. */
        let user_data: *mut c_void = ctx.as_mut() as *mut _ as *mut _;
        if let Some(ref context) = ctx.context {
            if let Ok(o) = context.get_server_info(server_info_cb, user_data) {
                ctx.operation_wait(None, &o);
            }
        }
        ctx.mainloop.unlock();

        // Return the result.
        Ok(ctx)
    }

    pub fn destroy(&mut self) {
        self.context_destroy();

        if !self.mainloop.is_null() {
            self.mainloop.stop();
        }
    }

    pub fn new_stream(&mut self,
                      stream_name: &CStr,
                      input_device: cubeb::DeviceId,
                      input_stream_params: Option<cubeb::StreamParams>,
                      output_device: cubeb::DeviceId,
                      output_stream_params: Option<cubeb::StreamParams>,
                      latency_frames: u32,
                      data_callback: cubeb::DataCallback,
                      state_callback: cubeb::StateCallback,
                      user_ptr: *mut c_void)
                      -> Result<Box<Stream>> {
        if self.error && self.context_init() != 0 {
            return Err(cubeb::ERROR);
        }

        Stream::new(self,
                    stream_name,
                    input_device,
                    input_stream_params,
                    output_device,
                    output_stream_params,
                    latency_frames,
                    data_callback,
                    state_callback,
                    user_ptr)
    }

    pub fn max_channel_count(&self) -> Result<u32> {
        match self.default_sink_info {
            Some(ref info) => Ok(info.channel_map.channels as u32),
            None => Err(cubeb::ERROR),
        }
    }

    pub fn preferred_sample_rate(&self) -> Result<u32> {
        match self.default_sink_info {
            Some(ref info) => Ok(info.sample_spec.rate),
            None => Err(cubeb::ERROR),
        }
    }

    pub fn min_latency(&self, params: &cubeb::StreamParams) -> Result<u32> {
        // According to PulseAudio developers, this is a safe minimum.
        Ok(25 * params.rate / 1000)
    }

    pub fn preferred_channel_layout(&self) -> Result<cubeb::ChannelLayout> {
        match self.default_sink_info {
            Some(ref info) => Ok(channel_map_to_layout(&info.channel_map)),
            None => Err(cubeb::ERROR),
        }
    }

    pub fn enumerate_devices(&self, devtype: cubeb::DeviceType) -> Result<cubeb::DeviceCollection> {
        fn add_output_device(_: &pulse::Context, i: *const pulse::SinkInfo, eol: i32, user_data: *mut c_void) {
            let list_data = unsafe { &mut *(user_data as *mut PulseDevListData) };
            let ctx = &(*list_data.context);

            if eol != 0 {
                ctx.mainloop.signal();
                return;
            }

            debug_assert!(!i.is_null());
            debug_assert!(!user_data.is_null());

            let info = unsafe { &*i };

            let group_id = match info.proplist().gets("sysfs.path") {
                Some(p) => p.to_owned().into_raw(),
                _ => ptr::null_mut(),
            };

            let vendor_name = match info.proplist().gets("device.vendor.name") {
                Some(p) => p.to_owned().into_raw(),
                _ => ptr::null_mut(),
            };

            let info_name = unsafe { CStr::from_ptr(info.name) };
            let info_description = unsafe { CStr::from_ptr(info.description) }.to_owned();

            let preferred = if *info_name == *list_data.default_sink_name {
                cubeb::DevicePref::ALL
            } else {
                cubeb::DevicePref::empty()
            };

            let device_id = ctx.devids.borrow_mut().add(info_name);
            let friendly_name = info_description.into_raw();
            let devinfo = cubeb::DeviceInfo {
                device_id: device_id,
                devid: device_id as cubeb::DeviceId,
                friendly_name: friendly_name,
                group_id: group_id,
                vendor_name: vendor_name,
                devtype: cubeb::DeviceType::OUTPUT,
                state: ctx.state_from_port(info.active_port),
                preferred: preferred,
                format: cubeb::DeviceFmt::all(),
                default_format: pulse_format_to_cubeb_format(info.sample_spec.format),
                max_channels: info.channel_map.channels as u32,
                min_rate: 1,
                max_rate: PA_RATE_MAX,
                default_rate: info.sample_spec.rate,
                latency_lo: 0,
                latency_hi: 0,
            };
            list_data.devinfo.push(devinfo);
        }

        fn add_input_device(_: &pulse::Context, i: *const pulse::SourceInfo, eol: i32, user_data: *mut c_void) {
            let list_data = unsafe { &mut *(user_data as *mut PulseDevListData) };
            let ctx = &(*list_data.context);

            if eol != 0 {
                ctx.mainloop.signal();
                return;
            }

            debug_assert!(!user_data.is_null());
            debug_assert!(!i.is_null());

            let info = unsafe { &*i };

            let group_id = match info.proplist().gets("sysfs.path") {
                Some(p) => p.to_owned().into_raw(),
                _ => ptr::null_mut(),
            };

            let vendor_name = match info.proplist().gets("device.vendor.name") {
                Some(p) => p.to_owned().into_raw(),
                _ => ptr::null_mut(),
            };

            let info_name = unsafe { CStr::from_ptr(info.name) };
            let info_description = unsafe { CStr::from_ptr(info.description) }.to_owned();

            let preferred = if *info_name == *list_data.default_source_name {
                cubeb::DevicePref::ALL
            } else {
                cubeb::DevicePref::empty()
            };

            let device_id = ctx.devids.borrow_mut().add(info_name);
            let friendly_name = info_description.into_raw();
            let devinfo = cubeb::DeviceInfo {
                device_id: device_id,
                devid: device_id as cubeb::DeviceId,
                friendly_name: friendly_name,
                group_id: group_id,
                vendor_name: vendor_name,
                devtype: cubeb::DeviceType::INPUT,
                state: ctx.state_from_port(info.active_port),
                preferred: preferred,
                format: cubeb::DeviceFmt::all(),
                default_format: pulse_format_to_cubeb_format(info.sample_spec.format),
                max_channels: info.channel_map.channels as u32,
                min_rate: 1,
                max_rate: PA_RATE_MAX,
                default_rate: info.sample_spec.rate,
                latency_lo: 0,
                latency_hi: 0,
            };

            list_data.devinfo.push(devinfo);

        }

        fn default_device_names(_: &pulse::Context, info: &pulse::ServerInfo, user_data: *mut c_void) {
            let list_data = unsafe { &mut *(user_data as *mut PulseDevListData) };

            list_data.default_sink_name = super::try_cstr_from(info.default_sink_name)
                .map(|s| s.to_owned())
                .unwrap_or_default();
            list_data.default_source_name = super::try_cstr_from(info.default_source_name)
                .map(|s| s.to_owned())
                .unwrap_or_default();

            (*list_data.context).mainloop.signal();
        }

        let mut user_data = PulseDevListData::new(self);

        if let Some(ref context) = self.context {
            self.mainloop.lock();

            if let Ok(o) = context.get_server_info(default_device_names, &mut user_data as *mut _ as *mut _) {
                self.operation_wait(None, &o);
            }

            if devtype.contains(cubeb::DeviceType::OUTPUT) {
                if let Ok(o) = context.get_sink_info_list(add_output_device, &mut user_data as *mut _ as *mut _) {
                    self.operation_wait(None, &o);
                }
            }

            if devtype.contains(cubeb::DeviceType::INPUT) {
                if let Ok(o) = context.get_source_info_list(add_input_device, &mut user_data as *mut _ as *mut _) {
                    self.operation_wait(None, &o);
                }
            }

            self.mainloop.unlock();
        }

        // Extract the array of cubeb_device_info from
        // PulseDevListData and convert it into C representation.
        let mut tmp = Vec::new();
        mem::swap(&mut user_data.devinfo, &mut tmp);
        let devices = tmp.into_boxed_slice();
        let coll = cubeb::DeviceCollection {
            device: devices.as_ptr(),
            count: devices.len(),
        };

        // Giving away the memory owned by devices.  Don't free it!
        mem::forget(devices);
        Ok(coll)
    }

    pub fn device_collection_destroy(&self, collection: *mut cubeb::DeviceCollection) {
        debug_assert!(!collection.is_null());
        unsafe {
            let coll = *collection;
            let mut devices = Vec::from_raw_parts(coll.device as *mut cubeb::DeviceInfo,
                                                  coll.count,
                                                  coll.count);
            for dev in devices.iter_mut() {
                if !dev.group_id.is_null() {
                    let _ = CString::from_raw(dev.group_id as *mut _);
                }
                if !dev.vendor_name.is_null() {
                    let _ = CString::from_raw(dev.vendor_name as *mut _);
                }
                if !dev.friendly_name.is_null() {
                    let _ = CString::from_raw(dev.friendly_name as *mut _);
                }
            }
        }
    }

    pub fn register_device_collection_changed(&mut self,
                                              devtype: cubeb::DeviceType,
                                              cb: cubeb::DeviceCollectionChangedCallback,
                                              user_ptr: *mut c_void)
                                              -> i32 {
        fn update_collection(_: &pulse::Context, event: pulse::SubscriptionEvent, index: u32, user_data: *mut c_void) {
            let ctx = unsafe { &mut *(user_data as *mut Context) };

            let (f, t) = (event.event_facility(), event.event_type());
            match f {
                pulse::SubscriptionEventFacility::Source |
                pulse::SubscriptionEventFacility::Sink => {
                    match t {
                        pulse::SubscriptionEventType::Remove |
                        pulse::SubscriptionEventType::New => {
                            if cubeb::log_enabled() {
                                let op = if t == pulse::SubscriptionEventType::New {
                                    "Adding"
                                } else {
                                    "Removing"
                                };
                                let dev = if f == pulse::SubscriptionEventFacility::Sink {
                                    "sink"
                                } else {
                                    "source "
                                };
                                log!("{} {} index {}", op, dev, index);

                                unsafe {
                                    ctx.collection_changed_callback.unwrap()(ctx as *mut _ as *mut _,
                                                                             ctx.collection_changed_user_ptr);
                                }
                            }
                        },
                        _ => {},
                    }
                },
                _ => {},
            }
        }

        fn success(_: &pulse::Context, success: i32, user_data: *mut c_void) {
            let ctx = unsafe { &*(user_data as *mut Context) };
            debug_assert_ne!(success, 0);
            ctx.mainloop.signal();
        }

        self.collection_changed_callback = cb;
        self.collection_changed_user_ptr = user_ptr;

        let user_data: *mut c_void = self as *mut _ as *mut _;
        if let Some(ref context) = self.context {
            self.mainloop.lock();

            let mut mask = pulse::SubscriptionMask::empty();
            if self.collection_changed_callback.is_none() {
                // Unregister subscription
                context.clear_subscribe_callback();
            } else {
                context.set_subscribe_callback(update_collection, user_data);
                if devtype.contains(cubeb::DeviceType::INPUT) {
                    mask |= pulse::SubscriptionMask::SOURCE
                };
                if devtype.contains(cubeb::DeviceType::OUTPUT) {
                    mask = pulse::SubscriptionMask::SINK
                };
            }

            if let Ok(o) = context.subscribe(mask, success, self as *const _ as *mut _) {
                self.operation_wait(None, &o);
            } else {
                self.mainloop.unlock();
                log!("Context subscribe failed");
                return cubeb::ERROR;
            }

            self.mainloop.unlock();
        }

        cubeb::OK
    }

    pub fn context_init(&mut self) -> i32 {
        fn error_state(c: &pulse::Context, u: *mut c_void) {
            let ctx = unsafe { &mut *(u as *mut Context) };
            if !c.get_state().is_good() {
                ctx.error = true;
            }
            ctx.mainloop.signal();
        }

        if self.context.is_some() {
            debug_assert!(self.error);
            self.context_destroy();
        }

        self.context = {
            let name = match self.context_name.as_ref() {
                Some(s) => Some(s.as_ref()),
                None => None,
            };
            pulse::Context::new(&self.mainloop.get_api(), name)
        };

        let context_ptr: *mut c_void = self as *mut _ as *mut _;
        if self.context.is_none() {
            return cubeb::ERROR;
        }

        self.mainloop.lock();
        if let Some(ref context) = self.context {
            context.set_state_callback(error_state, context_ptr);
            let _ = context.connect(None, pulse::ContextFlags::empty(), ptr::null());
        }

        if !self.wait_until_context_ready() {
            self.mainloop.unlock();
            self.context_destroy();
            return cubeb::ERROR;
        }

        self.mainloop.unlock();

        let version_str = unsafe { CStr::from_ptr(pulse::library_version()) };
        if let Ok(version) = semver::Version::parse(&version_str.to_string_lossy()) {
            self.version_0_9_8 = version >= semver::Version::parse("0.9.8").expect("Failed to parse version");
            self.version_2_0_0 = version >= semver::Version::parse("2.0.0").expect("Failed to parse version");
        }

        self.error = false;

        cubeb::OK
    }

    fn context_destroy(&mut self) {
        fn drain_complete(_: &pulse::Context, u: *mut c_void) {
            let ctx = unsafe { &*(u as *mut Context) };
            ctx.mainloop.signal();
        }

        let context_ptr: *mut c_void = self as *mut _ as *mut _;
        match self.context.take() {
            Some(ctx) => {
                self.mainloop.lock();
                if let Ok(o) = ctx.drain(drain_complete, context_ptr) {
                    self.operation_wait(None, &o);
                }
                ctx.clear_state_callback();
                ctx.disconnect();
                ctx.unref();
                self.mainloop.unlock();
            },
            _ => {},
        }
    }

    pub fn operation_wait<'a, S>(&self, s: S, o: &pulse::Operation) -> bool
        where S: Into<Option<&'a pulse::Stream>>
    {
        let stream = s.into();
        while o.get_state() == PA_OPERATION_RUNNING {
            self.mainloop.wait();
            if let Some(ref context) = self.context {
                if !context.get_state().is_good() {
                    return false;
                }
            }

            if let Some(stm) = stream {
                if !stm.get_state().is_good() {
                    return false;
                }
            }
        }

        true
    }

    pub fn wait_until_context_ready(&self) -> bool {
        if let Some(ref context) = self.context {
            loop {
                let state = context.get_state();
                if !state.is_good() {
                    return false;
                }
                if state == pulse::ContextState::Ready {
                    break;
                }
                self.mainloop.wait();
            }
        }

        true
    }

    fn state_from_port(&self, i: *const pa_port_info) -> cubeb::DeviceState {
        if !i.is_null() {
            let info = unsafe { *i };
            if self.version_2_0_0 && info.available == PA_PORT_AVAILABLE_NO {
                cubeb::DeviceState::Unplugged
            } else {
                cubeb::DeviceState::Enabled
            }
        } else {
            cubeb::DeviceState::Enabled
        }
    }
}

struct PulseDevListData<'a> {
    default_sink_name: CString,
    default_source_name: CString,
    devinfo: Vec<cubeb::DeviceInfo>,
    context: &'a Context,
}

impl<'a> PulseDevListData<'a> {
    pub fn new<'b>(context: &'b Context) -> Self
        where 'b: 'a
    {
        PulseDevListData {
            default_sink_name: CString::default(),
            default_source_name: CString::default(),
            devinfo: Vec::new(),
            context: context,
        }
    }
}

impl<'a> Drop for PulseDevListData<'a> {
    fn drop(&mut self) {
        for elem in &mut self.devinfo {
            let _ = unsafe { Box::from_raw(elem) };
        }
    }
}

fn pulse_format_to_cubeb_format(format: pa_sample_format_t) -> cubeb::DeviceFmt {
    match format {
        PA_SAMPLE_S16LE => cubeb::DeviceFmt::S16LE,
        PA_SAMPLE_S16BE => cubeb::DeviceFmt::S16BE,
        PA_SAMPLE_FLOAT32LE => cubeb::DeviceFmt::F32LE,
        PA_SAMPLE_FLOAT32BE => cubeb::DeviceFmt::F32BE,
        // Unsupported format, return F32NE
        _ => cubeb::DeviceFmt::F32NE,
    }
}
