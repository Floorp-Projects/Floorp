// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use backend::*;
use cubeb_backend::{
    ffi, log_enabled, Context, ContextOps, DeviceCollectionRef, DeviceId, DeviceType, Error, Ops,
    Result, Stream, StreamParams, StreamParamsRef,
};
use pulse::{self, ProplistExt};
use pulse_ffi::*;
use semver;
use std::cell::RefCell;
use std::default::Default;
use std::ffi::{CStr, CString};
use std::mem;
use std::os::raw::c_void;
use std::ptr;

#[derive(Debug)]
pub struct DefaultInfo {
    pub sample_spec: pulse::SampleSpec,
    pub channel_map: pulse::ChannelMap,
    pub flags: pulse::SinkFlags,
}

pub const PULSE_OPS: Ops = capi_new!(PulseContext, PulseStream);

#[repr(C)]
#[derive(Debug)]
pub struct PulseContext {
    _ops: *const Ops,
    pub mainloop: pulse::ThreadedMainloop,
    pub context: Option<pulse::Context>,
    pub default_sink_info: Option<DefaultInfo>,
    pub context_name: Option<CString>,
    pub input_collection_changed_callback: ffi::cubeb_device_collection_changed_callback,
    pub input_collection_changed_user_ptr: *mut c_void,
    pub output_collection_changed_callback: ffi::cubeb_device_collection_changed_callback,
    pub output_collection_changed_user_ptr: *mut c_void,
    pub error: bool,
    pub version_2_0_0: bool,
    pub version_0_9_8: bool,
    #[cfg(feature = "pulse-dlopen")]
    pub libpulse: LibLoader,
    devids: RefCell<Intern>,
}

impl PulseContext {
    #[cfg(feature = "pulse-dlopen")]
    fn _new(name: Option<CString>) -> Result<Box<Self>> {
        let libpulse = unsafe { open() };
        if libpulse.is_none() {
            cubeb_log!("libpulse not found");
            return Err(Error::error());
        }

        let ctx = Box::new(PulseContext {
            _ops: &PULSE_OPS,
            libpulse: libpulse.unwrap(),
            mainloop: pulse::ThreadedMainloop::new(),
            context: None,
            default_sink_info: None,
            context_name: name,
            input_collection_changed_callback: None,
            input_collection_changed_user_ptr: ptr::null_mut(),
            output_collection_changed_callback: None,
            output_collection_changed_user_ptr: ptr::null_mut(),
            error: true,
            version_0_9_8: false,
            version_2_0_0: false,
            devids: RefCell::new(Intern::new()),
        });

        Ok(ctx)
    }

    #[cfg(not(feature = "pulse-dlopen"))]
    fn _new(name: Option<CString>) -> Result<Box<Self>> {
        Ok(Box::new(PulseContext {
            _ops: &PULSE_OPS,
            mainloop: pulse::ThreadedMainloop::new(),
            context: None,
            default_sink_info: None,
            context_name: name,
            input_collection_changed_callback: None,
            input_collection_changed_user_ptr: ptr::null_mut(),
            output_collection_changed_callback: None,
            output_collection_changed_user_ptr: ptr::null_mut(),
            error: true,
            version_0_9_8: false,
            version_2_0_0: false,
            devids: RefCell::new(Intern::new()),
        }))
    }

    fn server_info_cb(context: &pulse::Context, info: Option<&pulse::ServerInfo>, u: *mut c_void) {
        fn sink_info_cb(_: &pulse::Context, i: *const pulse::SinkInfo, eol: i32, u: *mut c_void) {
            let ctx = unsafe { &mut *(u as *mut PulseContext) };
            if eol == 0 {
                let info = unsafe { &*i };
                let flags = pulse::SinkFlags::from_bits_truncate(info.flags);
                ctx.default_sink_info = Some(DefaultInfo {
                    sample_spec: info.sample_spec,
                    channel_map: info.channel_map,
                    flags,
                });
            }
            ctx.mainloop.signal();
        }

        if let Some(info) = info {
            let _ = context.get_sink_info_by_name(
                try_cstr_from(info.default_sink_name),
                sink_info_cb,
                u,
            );
        } else {
            // If info is None, then an error occured.
            let ctx = unsafe { &mut *(u as *mut PulseContext) };
            ctx.mainloop.signal();
        }
    }

    fn new(name: Option<&CStr>) -> Result<Box<Self>> {
        let name = name.map(|s| s.to_owned());
        let mut ctx = PulseContext::_new(name)?;

        if ctx.mainloop.start().is_err() {
            ctx.destroy();
            cubeb_log!("Error: couldn't start pulse's mainloop");
            return Err(Error::error());
        }

        if ctx.context_init().is_err() {
            ctx.destroy();
            cubeb_log!("Error: couldn't init pulse's context");
            return Err(Error::error());
        }

        ctx.mainloop.lock();
        /* server_info_callback performs a second async query,
         * which is responsible for initializing default_sink_info
         * and signalling the mainloop to end the wait. */
        let user_data: *mut c_void = ctx.as_mut() as *mut _ as *mut _;
        if let Some(ref context) = ctx.context {
            if let Ok(o) = context.get_server_info(PulseContext::server_info_cb, user_data) {
                ctx.operation_wait(None, &o);
            }
        }
        ctx.mainloop.unlock();

        /* Update `default_sink_info` when the default device changes. */
        if let Err(e) = ctx.subscribe_notifications(pulse::SubscriptionMask::SERVER) {
            cubeb_log!("subscribe_notifications ignored failure: {}", e);
        }

        // Return the result.
        Ok(ctx)
    }

    pub fn destroy(&mut self) {
        self.context_destroy();

        assert!(
            self.input_collection_changed_callback.is_none()
                && self.output_collection_changed_callback.is_none()
        );

        if !self.mainloop.is_null() {
            self.mainloop.stop();
        }
    }

    fn subscribe_notifications(&mut self, mask: pulse::SubscriptionMask) -> Result<()> {
        fn update_collection(
            _: &pulse::Context,
            event: pulse::SubscriptionEvent,
            index: u32,
            user_data: *mut c_void,
        ) {
            let ctx = unsafe { &mut *(user_data as *mut PulseContext) };

            let (f, t) = (event.event_facility(), event.event_type());
            if (f == pulse::SubscriptionEventFacility::Source)
                | (f == pulse::SubscriptionEventFacility::Sink)
            {
                if (t == pulse::SubscriptionEventType::Remove)
                    | (t == pulse::SubscriptionEventType::New)
                {
                    if log_enabled() {
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
                        cubeb_log!("{} {} index {}", op, dev, index);
                    }

                    if f == pulse::SubscriptionEventFacility::Source {
                        unsafe {
                            ctx.input_collection_changed_callback.unwrap()(
                                ctx as *mut _ as *mut _,
                                ctx.input_collection_changed_user_ptr,
                            );
                        }
                    }
                    if f == pulse::SubscriptionEventFacility::Sink {
                        unsafe {
                            ctx.output_collection_changed_callback.unwrap()(
                                ctx as *mut _ as *mut _,
                                ctx.output_collection_changed_user_ptr,
                            );
                        }
                    }
                }
            } else if (f == pulse::SubscriptionEventFacility::Server)
                && (t == pulse::SubscriptionEventType::Change)
            {
                cubeb_log!("Server changed {}", index as i32);
                let user_data: *mut c_void = ctx as *mut _ as *mut _;
                if let Some(ref context) = ctx.context {
                    if let Err(e) = context.get_server_info(PulseContext::server_info_cb, user_data)
                    {
                        cubeb_log!("Error: get_server_info ignored failure: {}", e);
                    }
                }
            }
        }

        fn success(_: &pulse::Context, success: i32, user_data: *mut c_void) {
            let ctx = unsafe { &*(user_data as *mut PulseContext) };
            if success != 1 {
                cubeb_log!("subscribe_success ignored failure: {}", success);
            }
            ctx.mainloop.signal();
        }

        let user_data: *mut c_void = self as *const _ as *mut _;
        if let Some(ref context) = self.context {
            self.mainloop.lock();

            context.set_subscribe_callback(update_collection, user_data);

            if let Ok(o) = context.subscribe(mask, success, self as *const _ as *mut _) {
                self.operation_wait(None, &o);
            } else {
                self.mainloop.unlock();
                cubeb_log!("Error: context subscribe failed");
                return Err(Error::error());
            }

            self.mainloop.unlock();
        }

        Ok(())
    }
}

impl ContextOps for PulseContext {
    fn init(context_name: Option<&CStr>) -> Result<Context> {
        let ctx = PulseContext::new(context_name)?;
        Ok(unsafe { Context::from_ptr(Box::into_raw(ctx) as *mut _) })
    }

    fn backend_id(&mut self) -> &'static CStr {
        unsafe { CStr::from_ptr(b"pulse-rust\0".as_ptr() as *const _) }
    }

    fn max_channel_count(&mut self) -> Result<u32> {
        match self.default_sink_info {
            Some(ref info) => Ok(u32::from(info.channel_map.channels)),
            None => {
                cubeb_log!("Error: couldn't get the max channel count");
                Err(Error::error())
            }
        }
    }

    fn min_latency(&mut self, params: StreamParams) -> Result<u32> {
        // According to PulseAudio developers, this is a safe minimum.
        Ok(25 * params.rate() / 1000)
    }

    fn preferred_sample_rate(&mut self) -> Result<u32> {
        match self.default_sink_info {
            Some(ref info) => Ok(info.sample_spec.rate),
            None => {
                cubeb_log!("Error: Couldn't get the preferred sample rate");
                Err(Error::error())
            }
        }
    }

    fn enumerate_devices(
        &mut self,
        devtype: DeviceType,
        collection: &DeviceCollectionRef,
    ) -> Result<()> {
        fn add_output_device(
            _: &pulse::Context,
            i: *const pulse::SinkInfo,
            eol: i32,
            user_data: *mut c_void,
        ) {
            let list_data = unsafe { &mut *(user_data as *mut PulseDevListData) };
            let ctx = list_data.context;

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
                ffi::CUBEB_DEVICE_PREF_ALL
            } else {
                ffi::CUBEB_DEVICE_PREF_NONE
            };

            let device_id = ctx.devids.borrow_mut().add(info_name);
            let friendly_name = info_description.into_raw();
            let devinfo = ffi::cubeb_device_info {
                device_id,
                devid: device_id as ffi::cubeb_devid,
                friendly_name,
                group_id,
                vendor_name,
                device_type: ffi::CUBEB_DEVICE_TYPE_OUTPUT,
                state: ctx.state_from_port(info.active_port),
                preferred,
                format: ffi::CUBEB_DEVICE_FMT_ALL,
                default_format: pulse_format_to_cubeb_format(info.sample_spec.format),
                max_channels: u32::from(info.channel_map.channels),
                min_rate: 1,
                max_rate: PA_RATE_MAX,
                default_rate: info.sample_spec.rate,
                latency_lo: 0,
                latency_hi: 0,
            };
            list_data.devinfo.push(devinfo);
        }

        fn add_input_device(
            _: &pulse::Context,
            i: *const pulse::SourceInfo,
            eol: i32,
            user_data: *mut c_void,
        ) {
            let list_data = unsafe { &mut *(user_data as *mut PulseDevListData) };
            let ctx = list_data.context;

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
                ffi::CUBEB_DEVICE_PREF_ALL
            } else {
                ffi::CUBEB_DEVICE_PREF_NONE
            };

            let device_id = ctx.devids.borrow_mut().add(info_name);
            let friendly_name = info_description.into_raw();
            let devinfo = ffi::cubeb_device_info {
                device_id,
                devid: device_id as ffi::cubeb_devid,
                friendly_name,
                group_id,
                vendor_name,
                device_type: ffi::CUBEB_DEVICE_TYPE_INPUT,
                state: ctx.state_from_port(info.active_port),
                preferred,
                format: ffi::CUBEB_DEVICE_FMT_ALL,
                default_format: pulse_format_to_cubeb_format(info.sample_spec.format),
                max_channels: u32::from(info.channel_map.channels),
                min_rate: 1,
                max_rate: PA_RATE_MAX,
                default_rate: info.sample_spec.rate,
                latency_lo: 0,
                latency_hi: 0,
            };

            list_data.devinfo.push(devinfo);
        }

        fn default_device_names(
            _: &pulse::Context,
            info: Option<&pulse::ServerInfo>,
            user_data: *mut c_void,
        ) {
            let list_data = unsafe { &mut *(user_data as *mut PulseDevListData) };

            if let Some(info) = info {
                list_data.default_sink_name = super::try_cstr_from(info.default_sink_name)
                    .map(|s| s.to_owned())
                    .unwrap_or_default();
                list_data.default_source_name = super::try_cstr_from(info.default_source_name)
                    .map(|s| s.to_owned())
                    .unwrap_or_default();
            }

            list_data.context.mainloop.signal();
        }

        let mut user_data = PulseDevListData::new(self);

        if let Some(ref context) = self.context {
            self.mainloop.lock();

            if let Ok(o) =
                context.get_server_info(default_device_names, &mut user_data as *mut _ as *mut _)
            {
                self.operation_wait(None, &o);
            }

            if devtype.contains(DeviceType::OUTPUT) {
                if let Ok(o) = context
                    .get_sink_info_list(add_output_device, &mut user_data as *mut _ as *mut _)
                {
                    self.operation_wait(None, &o);
                }
            }

            if devtype.contains(DeviceType::INPUT) {
                if let Ok(o) = context
                    .get_source_info_list(add_input_device, &mut user_data as *mut _ as *mut _)
                {
                    self.operation_wait(None, &o);
                }
            }

            self.mainloop.unlock();
        }

        // Extract the array of cubeb_device_info from
        // PulseDevListData and convert it into C representation.
        let mut tmp = Vec::new();
        mem::swap(&mut user_data.devinfo, &mut tmp);
        let mut devices = tmp.into_boxed_slice();
        let coll = unsafe { &mut *collection.as_ptr() };
        coll.device = devices.as_mut_ptr();
        coll.count = devices.len();

        // Giving away the memory owned by devices.  Don't free it!
        mem::forget(devices);
        Ok(())
    }

    fn device_collection_destroy(&mut self, collection: &mut DeviceCollectionRef) -> Result<()> {
        debug_assert!(!collection.as_ptr().is_null());
        unsafe {
            let coll = &mut *collection.as_ptr();
            let mut devices = Vec::from_raw_parts(coll.device, coll.count, coll.count);
            for dev in &mut devices {
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
            coll.device = ptr::null_mut();
            coll.count = 0;
        }
        Ok(())
    }

    #[cfg_attr(feature = "cargo-clippy", allow(clippy::too_many_arguments))]
    fn stream_init(
        &mut self,
        stream_name: Option<&CStr>,
        input_device: DeviceId,
        input_stream_params: Option<&StreamParamsRef>,
        output_device: DeviceId,
        output_stream_params: Option<&StreamParamsRef>,
        latency_frames: u32,
        data_callback: ffi::cubeb_data_callback,
        state_callback: ffi::cubeb_state_callback,
        user_ptr: *mut c_void,
    ) -> Result<Stream> {
        if self.error {
            self.context_init()?;
        }

        let stm = PulseStream::new(
            self,
            stream_name,
            input_device,
            input_stream_params,
            output_device,
            output_stream_params,
            latency_frames,
            data_callback,
            state_callback,
            user_ptr,
        )?;
        Ok(unsafe { Stream::from_ptr(Box::into_raw(stm) as *mut _) })
    }

    fn register_device_collection_changed(
        &mut self,
        devtype: DeviceType,
        cb: ffi::cubeb_device_collection_changed_callback,
        user_ptr: *mut c_void,
    ) -> Result<()> {
        if devtype.contains(DeviceType::INPUT) {
            self.input_collection_changed_callback = cb;
            self.input_collection_changed_user_ptr = user_ptr;
        }
        if devtype.contains(DeviceType::OUTPUT) {
            self.output_collection_changed_callback = cb;
            self.output_collection_changed_user_ptr = user_ptr;
        }

        let mut mask = pulse::SubscriptionMask::empty();
        if self.input_collection_changed_callback.is_some() {
            mask |= pulse::SubscriptionMask::SOURCE;
        }
        if self.output_collection_changed_callback.is_some() {
            mask |= pulse::SubscriptionMask::SINK;
        }
        /* Default device changed, this is always registered in order to update the
         * `default_sink_info` when the default device changes. */
        mask |= pulse::SubscriptionMask::SERVER;

        self.subscribe_notifications(mask)
    }
}

impl Drop for PulseContext {
    fn drop(&mut self) {
        self.destroy();
    }
}

impl PulseContext {
    /* Initialize PulseAudio Context */
    fn context_init(&mut self) -> Result<()> {
        fn error_state(c: &pulse::Context, u: *mut c_void) {
            let ctx = unsafe { &mut *(u as *mut PulseContext) };
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
            let name = self.context_name.as_ref().map(|s| s.as_ref());
            pulse::Context::new(&self.mainloop.get_api(), name)
        };

        let context_ptr: *mut c_void = self as *mut _ as *mut _;
        if self.context.is_none() {
            cubeb_log!("Error: couldn't create pulse's context");
            return Err(Error::error());
        }

        self.mainloop.lock();
        let connected = if let Some(ref context) = self.context {
            context.set_state_callback(error_state, context_ptr);
            context
                .connect(None, pulse::ContextFlags::empty(), ptr::null())
                .is_ok()
        } else {
            false
        };

        if !connected || !self.wait_until_context_ready() {
            self.mainloop.unlock();
            self.context_destroy();
            cubeb_log!("Error: error while waiting for pulse's context to be ready");
            return Err(Error::error());
        }

        self.mainloop.unlock();

        let version_str = unsafe { CStr::from_ptr(pulse::library_version()) };
        if let Ok(version) = semver::Version::parse(&version_str.to_string_lossy()) {
            self.version_0_9_8 =
                version >= semver::Version::parse("0.9.8").expect("Failed to parse version");
            self.version_2_0_0 =
                version >= semver::Version::parse("2.0.0").expect("Failed to parse version");
        }

        self.error = false;

        Ok(())
    }

    fn context_destroy(&mut self) {
        fn drain_complete(_: &pulse::Context, u: *mut c_void) {
            let ctx = unsafe { &*(u as *mut PulseContext) };
            ctx.mainloop.signal();
        }

        let context_ptr: *mut c_void = self as *mut _ as *mut _;
        if let Some(ctx) = self.context.take() {
            self.mainloop.lock();
            if let Ok(o) = ctx.drain(drain_complete, context_ptr) {
                self.operation_wait(None, &o);
            }
            ctx.clear_state_callback();
            ctx.disconnect();
            ctx.unref();
            self.mainloop.unlock();
        }
    }

    pub fn operation_wait<'a, S>(&self, s: S, o: &pulse::Operation) -> bool
    where
        S: Into<Option<&'a pulse::Stream>>,
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

    fn state_from_port(&self, i: *const pa_port_info) -> ffi::cubeb_device_state {
        if !i.is_null() {
            let info = unsafe { *i };
            if self.version_2_0_0 && info.available == PA_PORT_AVAILABLE_NO {
                ffi::CUBEB_DEVICE_STATE_UNPLUGGED
            } else {
                ffi::CUBEB_DEVICE_STATE_ENABLED
            }
        } else {
            ffi::CUBEB_DEVICE_STATE_ENABLED
        }
    }
}

struct PulseDevListData<'a> {
    default_sink_name: CString,
    default_source_name: CString,
    devinfo: Vec<ffi::cubeb_device_info>,
    context: &'a PulseContext,
}

impl<'a> PulseDevListData<'a> {
    pub fn new<'b>(context: &'b PulseContext) -> Self
    where
        'b: 'a,
    {
        PulseDevListData {
            default_sink_name: CString::default(),
            default_source_name: CString::default(),
            devinfo: Vec::new(),
            context,
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

fn pulse_format_to_cubeb_format(format: pa_sample_format_t) -> ffi::cubeb_device_fmt {
    match format {
        PA_SAMPLE_S16LE => ffi::CUBEB_DEVICE_FMT_S16LE,
        PA_SAMPLE_S16BE => ffi::CUBEB_DEVICE_FMT_S16BE,
        PA_SAMPLE_FLOAT32LE => ffi::CUBEB_DEVICE_FMT_F32LE,
        PA_SAMPLE_FLOAT32BE => ffi::CUBEB_DEVICE_FMT_F32BE,
        // Unsupported format, return F32NE
        _ => ffi::CUBEB_DEVICE_FMT_F32NE,
    }
}
