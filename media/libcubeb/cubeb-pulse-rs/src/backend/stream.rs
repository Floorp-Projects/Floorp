// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use backend::*;
use backend::cork_state::CorkState;
use cubeb_backend::{ffi, log_enabled, ChannelLayout, DeviceId, DeviceRef, Error, Result,
                    SampleFormat, StreamOps, StreamParamsRef, StreamPrefs};
use pulse::{self, CVolumeExt, ChannelMapExt, SampleSpecExt, StreamLatency, USecExt};
use pulse_ffi::*;
use std::{mem, ptr};
use std::ffi::{CStr, CString};
use std::os::raw::{c_long, c_void};

const PULSE_NO_GAIN: f32 = -1.0;

/// Iterator interface to `ChannelLayout`.
///
/// Iterates each channel in the set represented by `ChannelLayout`.
struct ChannelLayoutIter {
    /// The layout set being iterated
    layout: ChannelLayout,
    /// The next flag to test
    index: u8,
}

fn channel_layout_iter(layout: ChannelLayout) -> ChannelLayoutIter {
    let index = 0;
    ChannelLayoutIter { layout, index }
}

impl Iterator for ChannelLayoutIter {
    type Item = ChannelLayout;

    fn next(&mut self) -> Option<Self::Item> {
        while !self.layout.is_empty() {
            let test = Self::Item::from_bits_truncate(1 << self.index);
            self.index += 1;
            if self.layout.contains(test) {
                self.layout.remove(test);
                return Some(test);
            }
        }
        None
    }
}

fn cubeb_channel_to_pa_channel(channel: ffi::cubeb_channel) -> pa_channel_position_t {
    match channel {
        ffi::CHANNEL_FRONT_LEFT => PA_CHANNEL_POSITION_FRONT_LEFT,
        ffi::CHANNEL_FRONT_RIGHT => PA_CHANNEL_POSITION_FRONT_RIGHT,
        ffi::CHANNEL_FRONT_CENTER => PA_CHANNEL_POSITION_FRONT_CENTER,
        ffi::CHANNEL_LOW_FREQUENCY => PA_CHANNEL_POSITION_LFE,
        ffi::CHANNEL_BACK_LEFT => PA_CHANNEL_POSITION_REAR_LEFT,
        ffi::CHANNEL_BACK_RIGHT => PA_CHANNEL_POSITION_REAR_RIGHT,
        ffi::CHANNEL_FRONT_LEFT_OF_CENTER => PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER,
        ffi::CHANNEL_FRONT_RIGHT_OF_CENTER => PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER,
        ffi::CHANNEL_BACK_CENTER => PA_CHANNEL_POSITION_REAR_CENTER,
        ffi::CHANNEL_SIDE_LEFT => PA_CHANNEL_POSITION_SIDE_LEFT,
        ffi::CHANNEL_SIDE_RIGHT => PA_CHANNEL_POSITION_SIDE_RIGHT,
        ffi::CHANNEL_TOP_CENTER => PA_CHANNEL_POSITION_TOP_CENTER,
        ffi::CHANNEL_TOP_FRONT_LEFT => PA_CHANNEL_POSITION_TOP_FRONT_LEFT,
        ffi::CHANNEL_TOP_FRONT_CENTER => PA_CHANNEL_POSITION_TOP_FRONT_CENTER,
        ffi::CHANNEL_TOP_FRONT_RIGHT => PA_CHANNEL_POSITION_TOP_FRONT_RIGHT,
        ffi::CHANNEL_TOP_BACK_LEFT => PA_CHANNEL_POSITION_TOP_REAR_LEFT,
        ffi::CHANNEL_TOP_BACK_CENTER => PA_CHANNEL_POSITION_TOP_REAR_CENTER,
        ffi::CHANNEL_TOP_BACK_RIGHT => PA_CHANNEL_POSITION_TOP_REAR_RIGHT,
        _ => PA_CHANNEL_POSITION_INVALID,
    }
}

fn layout_to_channel_map(layout: ChannelLayout) -> pulse::ChannelMap {
    assert_ne!(layout, ChannelLayout::UNDEFINED);

    let mut cm = pulse::ChannelMap::init();
    for (i, channel) in channel_layout_iter(layout).enumerate() {
        cm.map[i] = cubeb_channel_to_pa_channel(channel.into());
    }
    cm.channels = layout.num_channels() as _;
    cm
}

pub struct Device(ffi::cubeb_device);

impl Drop for Device {
    fn drop(&mut self) {
        unsafe {
            if !self.0.input_name.is_null() {
                let _ = CString::from_raw(self.0.input_name as *mut _);
            }
            if !self.0.output_name.is_null() {
                let _ = CString::from_raw(self.0.output_name as *mut _);
            }
        }
    }
}

#[repr(C)]
#[derive(Debug)]
pub struct PulseStream<'ctx> {
    context: &'ctx PulseContext,
    user_ptr: *mut c_void,
    output_stream: Option<pulse::Stream>,
    input_stream: Option<pulse::Stream>,
    data_callback: ffi::cubeb_data_callback,
    state_callback: ffi::cubeb_state_callback,
    drain_timer: *mut pa_time_event,
    output_sample_spec: pulse::SampleSpec,
    input_sample_spec: pulse::SampleSpec,
    shutdown: bool,
    volume: f32,
    state: ffi::cubeb_state,
}

impl<'ctx> PulseStream<'ctx> {
    #[cfg_attr(feature = "cargo-clippy", allow(too_many_arguments))]
    pub fn new(
        context: &'ctx PulseContext,
        stream_name: Option<&CStr>,
        input_device: DeviceId,
        input_stream_params: Option<&StreamParamsRef>,
        output_device: DeviceId,
        output_stream_params: Option<&StreamParamsRef>,
        latency_frames: u32,
        data_callback: ffi::cubeb_data_callback,
        state_callback: ffi::cubeb_state_callback,
        user_ptr: *mut c_void,
    ) -> Result<Box<Self>> {
        fn check_error(s: &pulse::Stream, u: *mut c_void) {
            let stm = unsafe { &mut *(u as *mut PulseStream) };
            if !s.get_state().is_good() {
                stm.state_change_callback(ffi::CUBEB_STATE_ERROR);
            }
            stm.context.mainloop.signal();
        }

        fn read_data(s: &pulse::Stream, nbytes: usize, u: *mut c_void) {
            fn read_from_input(
                s: &pulse::Stream,
                buffer: *mut *const c_void,
                size: *mut usize,
            ) -> i32 {
                let readable_size: i32 = s.readable_size().and_then(|s| Ok(s as i32)).unwrap_or(-1);
                if readable_size > 0 && unsafe { s.peek(buffer, size).is_err() } {
                    return -1;
                }
                readable_size
            }

            cubeb_logv!("Input callback buffer size {}", nbytes);
            let stm = unsafe { &mut *(u as *mut PulseStream) };
            if stm.shutdown {
                return;
            }

            let mut read_data: *const c_void = ptr::null();
            let mut read_size: usize = 0;
            while read_from_input(s, &mut read_data, &mut read_size) > 0 {
                /* read_data can be NULL in case of a hole. */
                if !read_data.is_null() {
                    let in_frame_size = stm.input_sample_spec.frame_size();
                    let read_frames = read_size / in_frame_size;

                    if stm.output_stream.is_some() {
                        // input/capture + output/playback operation
                        let out_frame_size = stm.output_sample_spec.frame_size();
                        let write_size = read_frames * out_frame_size;
                        // Offer full duplex data for writing
                        stm.trigger_user_callback(read_data, write_size);
                    } else {
                        // input/capture only operation. Call callback directly
                        let got = unsafe {
                            stm.data_callback.unwrap()(
                                stm as *mut _ as *mut _,
                                stm.user_ptr,
                                read_data,
                                ptr::null_mut(),
                                read_frames as c_long,
                            )
                        };

                        if got < 0 || got as usize != read_frames {
                            let _ = s.cancel_write();
                            stm.shutdown = true;
                            break;
                        }
                    }
                }

                if read_size > 0 {
                    let _ = s.drop();
                }

                if stm.shutdown {
                    return;
                }
            }
        }

        fn write_data(_: &pulse::Stream, nbytes: usize, u: *mut c_void) {
            cubeb_logv!("Output callback to be written buffer size {}", nbytes);
            let stm = unsafe { &mut *(u as *mut PulseStream) };
            if stm.shutdown || stm.state != ffi::CUBEB_STATE_STARTED {
                return;
            }

            if stm.input_stream.is_none() {
                // Output/playback only operation.
                // Write directly to output
                debug_assert!(stm.output_stream.is_some());
                stm.trigger_user_callback(ptr::null(), nbytes);
            }
        }

        let mut stm = Box::new(PulseStream {
            context: context,
            output_stream: None,
            input_stream: None,
            data_callback: data_callback,
            state_callback: state_callback,
            user_ptr: user_ptr,
            drain_timer: ptr::null_mut(),
            output_sample_spec: pulse::SampleSpec::default(),
            input_sample_spec: pulse::SampleSpec::default(),
            shutdown: false,
            volume: PULSE_NO_GAIN,
            state: ffi::CUBEB_STATE_ERROR,
        });

        if let Some(ref context) = stm.context.context {
            stm.context.mainloop.lock();

            // Setup output stream
            if let Some(stream_params) = output_stream_params {
                match PulseStream::stream_init(context, stream_params, stream_name) {
                    Ok(s) => {
                        stm.output_sample_spec = *s.get_sample_spec();

                        s.set_state_callback(check_error, stm.as_mut() as *mut _ as *mut _);
                        s.set_write_callback(write_data, stm.as_mut() as *mut _ as *mut _);

                        let battr =
                            set_buffering_attribute(latency_frames, &stm.output_sample_spec);
                        let device_name = super::try_cstr_from(output_device as *const _);
                        let _ = s.connect_playback(
                            device_name,
                            &battr,
                            pulse::StreamFlags::AUTO_TIMING_UPDATE
                                | pulse::StreamFlags::INTERPOLATE_TIMING
                                | pulse::StreamFlags::START_CORKED
                                | pulse::StreamFlags::ADJUST_LATENCY,
                            None,
                            None,
                        );

                        stm.output_stream = Some(s);
                    }
                    Err(e) => {
                        stm.context.mainloop.unlock();
                        stm.destroy();
                        return Err(e);
                    }
                }
            }

            // Set up input stream
            if let Some(ref stream_params) = input_stream_params {
                match PulseStream::stream_init(context, stream_params, stream_name) {
                    Ok(s) => {
                        stm.input_sample_spec = *s.get_sample_spec();

                        s.set_state_callback(check_error, stm.as_mut() as *mut _ as *mut _);
                        s.set_read_callback(read_data, stm.as_mut() as *mut _ as *mut _);

                        let battr = set_buffering_attribute(latency_frames, &stm.input_sample_spec);
                        let device_name = super::try_cstr_from(input_device as *const _);
                        let _ = s.connect_record(
                            device_name,
                            &battr,
                            pulse::StreamFlags::AUTO_TIMING_UPDATE
                                | pulse::StreamFlags::INTERPOLATE_TIMING
                                | pulse::StreamFlags::START_CORKED
                                | pulse::StreamFlags::ADJUST_LATENCY,
                        );

                        stm.input_stream = Some(s);
                    }
                    Err(e) => {
                        stm.context.mainloop.unlock();
                        stm.destroy();
                        return Err(e);
                    }
                }
            }

            let r = if stm.wait_until_ready() {
                /* force a timing update now, otherwise timing info does not become valid
                until some point after initialization has completed. */
                stm.update_timing_info()
            } else {
                false
            };

            stm.context.mainloop.unlock();

            if !r {
                stm.destroy();
                return Err(Error::error());
            }

            // TODO:
            if log_enabled() {
                if let Some(ref output_stream) = stm.output_stream {
                    let output_att = output_stream.get_buffer_attr();
                    cubeb_log!(
                        "Output buffer attributes maxlength {}, tlength {}, \
                         prebuf {}, minreq {}, fragsize {}",
                        output_att.maxlength,
                        output_att.tlength,
                        output_att.prebuf,
                        output_att.minreq,
                        output_att.fragsize
                    );
                }

                if let Some(ref input_stream) = stm.input_stream {
                    let input_att = input_stream.get_buffer_attr();
                    cubeb_log!(
                        "Input buffer attributes maxlength {}, tlength {}, \
                         prebuf {}, minreq {}, fragsize {}",
                        input_att.maxlength,
                        input_att.tlength,
                        input_att.prebuf,
                        input_att.minreq,
                        input_att.fragsize
                    );
                }
            }
        }

        Ok(stm)
    }

    fn destroy(&mut self) {
        self.cork(CorkState::cork());

        self.context.mainloop.lock();
        {
            if let Some(stm) = self.output_stream.take() {
                if !self.drain_timer.is_null() {
                    /* there's no pa_rttime_free, so use this instead. */
                    self.context.mainloop.get_api().time_free(self.drain_timer);
                }
                stm.clear_state_callback();
                stm.clear_write_callback();
                let _ = stm.disconnect();
                stm.unref();
            }

            if let Some(stm) = self.input_stream.take() {
                stm.clear_state_callback();
                stm.clear_read_callback();
                let _ = stm.disconnect();
                stm.unref();
            }
        }
        self.context.mainloop.unlock();
    }
}

impl<'ctx> Drop for PulseStream<'ctx> {
    fn drop(&mut self) {
        self.destroy();
    }
}

impl<'ctx> StreamOps for PulseStream<'ctx> {
    fn start(&mut self) -> Result<()> {
        fn output_preroll(_: &pulse::MainloopApi, u: *mut c_void) {
            let stm = unsafe { &mut *(u as *mut PulseStream) };
            if !stm.shutdown {
                let size = stm.output_stream
                    .as_ref()
                    .map_or(0, |s| s.writable_size().unwrap_or(0));
                stm.trigger_user_callback(ptr::null_mut(), size);
            }
        }

        self.shutdown = false;
        self.cork(CorkState::uncork() | CorkState::notify());

        if self.output_stream.is_some() && self.input_stream.is_none() {
            /* On output only case need to manually call user cb once in order to make
             * things roll. This is done via a defer event in order to execute it
             * from PA server thread. */
            self.context.mainloop.lock();
            self.context
                .mainloop
                .get_api()
                .once(output_preroll, self as *const _ as *mut _);
            self.context.mainloop.unlock();
        }

        Ok(())
    }

    fn stop(&mut self) -> Result<()> {
        {
            self.context.mainloop.lock();
            self.shutdown = true;
            // If draining is taking place wait to finish
            while !self.drain_timer.is_null() {
                self.context.mainloop.wait();
            }
            self.context.mainloop.unlock();
        }
        self.cork(CorkState::cork() | CorkState::notify());

        Ok(())
    }

    fn reset_default_device(&mut self) -> Result<()> {
        Err(not_supported())
    }

    fn position(&mut self) -> Result<u64> {
        let in_thread = self.context.mainloop.in_thread();

        if !in_thread {
            self.context.mainloop.lock();
        }

        if self.output_stream.is_none() {
            return Err(Error::error());
        }

        let stm = self.output_stream.as_ref().unwrap();
        let r = match stm.get_time() {
            Ok(r_usec) => {
                let bytes = r_usec.to_bytes(&self.output_sample_spec);
                Ok((bytes / self.output_sample_spec.frame_size()) as u64)
            }
            Err(_) => Err(Error::error()),
        };

        if !in_thread {
            self.context.mainloop.unlock();
        }

        r
    }

    fn latency(&mut self) -> Result<u32> {
        match self.output_stream {
            None => Err(Error::error()),
            Some(ref stm) => match stm.get_latency() {
                Ok(StreamLatency::Positive(r_usec)) => {
                    let latency = (r_usec * pa_usec_t::from(self.output_sample_spec.rate)
                        / PA_USEC_PER_SEC) as u32;
                    Ok(latency)
                }
                Ok(_) => {
                    panic!("Can not handle negative latency values.");
                }
                Err(_) => Err(Error::error()),
            },
        }
    }

    fn set_volume(&mut self, volume: f32) -> Result<()> {
        match self.output_stream {
            None => Err(Error::error()),
            Some(ref stm) => {
                if let Some(ref context) = self.context.context {
                    self.context.mainloop.lock();

                    let mut cvol: pa_cvolume = Default::default();

                    /* if the pulse daemon is configured to use flat
                     * volumes, apply our own gain instead of changing
                     * the input volume on the sink. */
                    let flags = {
                        match self.context.default_sink_info {
                            Some(ref info) => info.flags,
                            _ => pulse::SinkFlags::empty(),
                        }
                    };

                    if flags.contains(pulse::SinkFlags::FLAT_VOLUME) {
                        self.volume = volume;
                    } else {
                        let channels = stm.get_sample_spec().channels;
                        let vol = pulse::sw_volume_from_linear(f64::from(volume));
                        cvol.set(u32::from(channels), vol);

                        let index = stm.get_index();

                        let context_ptr = self.context as *const _ as *mut _;
                        if let Ok(o) = context.set_sink_input_volume(
                            index,
                            &cvol,
                            context_success,
                            context_ptr,
                        ) {
                            self.context.operation_wait(stm, &o);
                        }
                    }

                    self.context.mainloop.unlock();
                    Ok(())
                } else {
                    Err(Error::error())
                }
            }
        }
    }

    fn set_panning(&mut self, panning: f32) -> Result<()> {
        #[repr(C)]
        struct SinkInputInfoResult<'a> {
            pub cvol: pulse::CVolume,
            pub mainloop: &'a pulse::ThreadedMainloop,
        }

        fn get_input_volume(
            _: &pulse::Context,
            info: *const pulse::SinkInputInfo,
            eol: i32,
            u: *mut c_void,
        ) {
            let r = unsafe { &mut *(u as *mut SinkInputInfoResult) };
            if eol == 0 {
                let info = unsafe { *info };
                r.cvol = info.volume;
            }
            r.mainloop.signal();
        }

        if self.output_stream.is_none() {
            return Err(Error::error());
        }

        let stm = self.output_stream.as_ref().unwrap();

        if let Some(ref context) = self.context.context {
            self.context.mainloop.lock();

            let map = stm.get_channel_map();
            if !map.can_balance() {
                self.context.mainloop.unlock();
                return Err(Error::error());
            }

            let index = stm.get_index();

            let mut r = SinkInputInfoResult {
                cvol: pulse::CVolume::default(),
                mainloop: &self.context.mainloop,
            };

            if let Ok(o) =
                context.get_sink_input_info(index, get_input_volume, &mut r as *mut _ as *mut _)
            {
                self.context.operation_wait(stm, &o);
            }

            r.cvol.set_balance(map, panning);

            let context_ptr = self.context as *const _ as *mut _;
            if let Ok(o) =
                context.set_sink_input_volume(index, &r.cvol, context_success, context_ptr)
            {
                self.context.operation_wait(stm, &o);
            }

            self.context.mainloop.unlock();

            Ok(())
        } else {
            Err(Error::error())
        }
    }

    fn current_device(&mut self) -> Result<&DeviceRef> {
        if self.context.version_0_9_8 {
            let mut dev: Box<ffi::cubeb_device> = Box::new(unsafe { mem::zeroed() });

            if let Some(ref stm) = self.input_stream {
                dev.input_name = match stm.get_device_name() {
                    Ok(name) => name.to_owned().into_raw(),
                    Err(_) => {
                        return Err(Error::error());
                    }
                }
            }

            if let Some(ref stm) = self.output_stream {
                dev.output_name = match stm.get_device_name() {
                    Ok(name) => name.to_owned().into_raw(),
                    Err(_) => {
                        return Err(Error::error());
                    }
                }
            }

            Ok(unsafe { DeviceRef::from_ptr(Box::into_raw(dev) as *mut _) })
        } else {
            Err(not_supported())
        }
    }

    fn device_destroy(&mut self, device: &DeviceRef) -> Result<()> {
        if device.as_ptr().is_null() {
            Err(Error::error())
        } else {
            unsafe {
                let _: Box<Device> = Box::from_raw(device.as_ptr() as *mut _);
            }
            Ok(())
        }
    }

    fn register_device_changed_callback(
        &mut self,
        _: ffi::cubeb_device_changed_callback,
    ) -> Result<()> {
        Err(Error::error())
    }
}

impl<'ctx> PulseStream<'ctx> {
    fn stream_init(
        context: &pulse::Context,
        stream_params: &StreamParamsRef,
        stream_name: Option<&CStr>,
    ) -> Result<pulse::Stream> {
        if stream_params.prefs() == StreamPrefs::LOOPBACK {
            return Err(not_supported());
        }

        fn to_pulse_format(format: SampleFormat) -> pulse::SampleFormat {
            match format {
                SampleFormat::S16LE => pulse::SampleFormat::Signed16LE,
                SampleFormat::S16BE => pulse::SampleFormat::Signed16BE,
                SampleFormat::Float32LE => pulse::SampleFormat::Float32LE,
                SampleFormat::Float32BE => pulse::SampleFormat::Float32BE,
                _ => pulse::SampleFormat::Invalid,
            }
        }

        let fmt = to_pulse_format(stream_params.format());
        if fmt == pulse::SampleFormat::Invalid {
            return Err(invalid_format());
        }

        let ss = pulse::SampleSpec {
            channels: stream_params.channels() as u8,
            format: fmt.into(),
            rate: stream_params.rate(),
        };

        let cm: Option<pa_channel_map> = match stream_params.layout() {
            ChannelLayout::UNDEFINED => None,
            _ => Some(layout_to_channel_map(stream_params.layout())),
        };

        let stream = pulse::Stream::new(context, stream_name.unwrap(), &ss, cm.as_ref());

        match stream {
            None => Err(Error::error()),
            Some(stm) => Ok(stm),
        }
    }

    pub fn cork_stream(&self, stream: Option<&pulse::Stream>, state: CorkState) {
        if let Some(stm) = stream {
            if let Ok(o) = stm.cork(
                state.is_cork() as i32,
                stream_success,
                self as *const _ as *mut _,
            ) {
                self.context.operation_wait(stream, &o);
            }
        }
    }

    fn cork(&mut self, state: CorkState) {
        {
            self.context.mainloop.lock();
            self.cork_stream(self.output_stream.as_ref(), state);
            self.cork_stream(self.input_stream.as_ref(), state);
            self.context.mainloop.unlock()
        }

        if state.is_notify() {
            self.state_change_callback(if state.is_cork() {
                ffi::CUBEB_STATE_STOPPED
            } else {
                ffi::CUBEB_STATE_STARTED
            });
        }
    }

    fn update_timing_info(&self) -> bool {
        let mut r = false;

        if let Some(ref stm) = self.output_stream {
            if let Ok(o) = stm.update_timing_info(stream_success, self as *const _ as *mut _) {
                r = self.context.operation_wait(stm, &o);
            }

            if !r {
                return r;
            }
        }

        if let Some(ref stm) = self.input_stream {
            if let Ok(o) = stm.update_timing_info(stream_success, self as *const _ as *mut _) {
                r = self.context.operation_wait(stm, &o);
            }
        }

        r
    }

    pub fn state_change_callback(&mut self, s: ffi::cubeb_state) {
        self.state = s;
        unsafe {
            (self.state_callback.unwrap())(
                self as *mut PulseStream as *mut ffi::cubeb_stream,
                self.user_ptr,
                s,
            )
        };
    }

    fn wait_until_ready(&self) -> bool {
        fn wait_until_io_stream_ready(
            stm: &pulse::Stream,
            mainloop: &pulse::ThreadedMainloop,
        ) -> bool {
            if mainloop.is_null() {
                return false;
            }

            loop {
                let state = stm.get_state();
                if !state.is_good() {
                    return false;
                }
                if state == pulse::StreamState::Ready {
                    break;
                }
                mainloop.wait();
            }

            true
        }

        if let Some(ref stm) = self.output_stream {
            if !wait_until_io_stream_ready(stm, &self.context.mainloop) {
                return false;
            }
        }

        if let Some(ref stm) = self.input_stream {
            if !wait_until_io_stream_ready(stm, &self.context.mainloop) {
                return false;
            }
        }

        true
    }

    #[cfg_attr(feature = "cargo-clippy", allow(cyclomatic_complexity))]
    fn trigger_user_callback(&mut self, input_data: *const c_void, nbytes: usize) {
        fn drained_cb(
            a: &pulse::MainloopApi,
            e: *mut pa_time_event,
            _tv: &pulse::TimeVal,
            u: *mut c_void,
        ) {
            let stm = unsafe { &mut *(u as *mut PulseStream) };
            debug_assert_eq!(stm.drain_timer, e);
            stm.state_change_callback(ffi::CUBEB_STATE_DRAINED);
            /* there's no pa_rttime_free, so use this instead. */
            a.time_free(stm.drain_timer);
            stm.drain_timer = ptr::null_mut();
            stm.context.mainloop.signal();
        }

        if let Some(ref stm) = self.output_stream {
            let frame_size = self.output_sample_spec.frame_size();
            debug_assert_eq!(nbytes % frame_size, 0);

            let mut towrite = nbytes;
            let mut read_offset = 0usize;
            while towrite > 0 {
                match stm.begin_write(towrite) {
                    Err(e) => {
                        panic!("Failed to write data: {}", e);
                    }
                    Ok((buffer, size)) => {
                        debug_assert!(size > 0);
                        debug_assert_eq!(size % frame_size, 0);

                        cubeb_logv!(
                            "Trigger user callback with output buffer size={}, read_offset={}",
                            size,
                            read_offset
                        );
                        let read_ptr =
                            unsafe { (input_data as *const u8).offset(read_offset as isize) };
                        let got = unsafe {
                            self.data_callback.unwrap()(
                                self as *const _ as *mut _,
                                self.user_ptr,
                                read_ptr as *const _ as *mut _,
                                buffer,
                                (size / frame_size) as c_long,
                            )
                        };
                        if got < 0 {
                            let _ = stm.cancel_write();
                            self.shutdown = true;
                            return;
                        }

                        // If more iterations move offset of read buffer
                        if !input_data.is_null() {
                            let in_frame_size = self.input_sample_spec.frame_size();
                            read_offset += (size / frame_size) * in_frame_size;
                        }

                        if self.volume != PULSE_NO_GAIN {
                            let samples = (self.output_sample_spec.channels as usize * size
                                / frame_size) as isize;

                            if self.output_sample_spec.format == PA_SAMPLE_S16BE
                                || self.output_sample_spec.format == PA_SAMPLE_S16LE
                            {
                                let b = buffer as *mut i16;
                                for i in 0..samples {
                                    unsafe { *b.offset(i) *= self.volume as i16 };
                                }
                            } else {
                                let b = buffer as *mut f32;
                                for i in 0..samples {
                                    unsafe { *b.offset(i) *= self.volume };
                                }
                            }
                        }

                        let r = stm.write(
                            buffer,
                            got as usize * frame_size,
                            0,
                            pulse::SeekMode::Relative,
                        );
                        debug_assert!(r.is_ok());

                        if (got as usize) < size / frame_size {
                            let latency = match stm.get_latency() {
                                Ok(StreamLatency::Positive(l)) => l,
                                Ok(_) => {
                                    panic!("Can not handle negative latency values.");
                                }
                                Err(e) => {
                                    debug_assert_eq!(
                                        e,
                                        pulse::ErrorCode::from_error_code(PA_ERR_NODATA)
                                    );
                                    /* this needs a better guess. */
                                    100 * PA_USEC_PER_MSEC
                                }
                            };

                            /* pa_stream_drain is useless, see PA bug# 866. this is a workaround. */
                            /* arbitrary safety margin: double the current latency. */
                            debug_assert!(self.drain_timer.is_null());
                            let stream_ptr = self as *const _ as *mut _;
                            if let Some(ref context) = self.context.context {
                                self.drain_timer = context.rttime_new(
                                    pulse::rtclock_now() + 2 * latency,
                                    drained_cb,
                                    stream_ptr,
                                );
                            }
                            self.shutdown = true;
                            return;
                        }

                        towrite -= size;
                    }
                }
            }
            debug_assert_eq!(towrite, 0);
        }
    }
}

fn stream_success(_: &pulse::Stream, success: i32, u: *mut c_void) {
    let stm = unsafe { &*(u as *mut PulseStream) };
    debug_assert_ne!(success, 0);
    stm.context.mainloop.signal();
}

fn context_success(_: &pulse::Context, success: i32, u: *mut c_void) {
    let ctx = unsafe { &*(u as *mut PulseContext) };
    debug_assert_ne!(success, 0);
    ctx.mainloop.signal();
}

fn set_buffering_attribute(latency_frames: u32, sample_spec: &pa_sample_spec) -> pa_buffer_attr {
    let tlength = latency_frames * sample_spec.frame_size() as u32;
    let minreq = tlength / 4;
    let battr = pa_buffer_attr {
        maxlength: u32::max_value(),
        prebuf: u32::max_value(),
        tlength: tlength,
        minreq: minreq,
        fragsize: minreq,
    };

    cubeb_log!(
        "Requested buffer attributes maxlength {}, tlength {}, prebuf {}, minreq {}, fragsize {}",
        battr.maxlength,
        battr.tlength,
        battr.prebuf,
        battr.minreq,
        battr.fragsize
    );

    battr
}

fn invalid_format() -> Error {
    unsafe { Error::from_raw(ffi::CUBEB_ERROR_INVALID_FORMAT) }
}

fn not_supported() -> Error {
    unsafe { Error::from_raw(ffi::CUBEB_ERROR_NOT_SUPPORTED) }
}

#[cfg(all(test, not(feature = "pulse-dlopen")))]
mod test {
    use cubeb_backend::ChannelLayout;
    use pulse_ffi::*;
    use super::layout_to_channel_map;

    macro_rules! channel_tests {
        {$($name: ident, $layout: ident => [ $($channels: ident),* ]),+} => {
            $(
            #[test]
            fn $name() {
                let layout = ChannelLayout::$layout;
                let mut iter = super::channel_layout_iter(layout);
                $(
                assert_eq!(Some(ChannelLayout::$channels), iter.next());
                )*
                assert_eq!(None, iter.next());
            }

            )*
        }
    }

    channel_tests! {
        channels_unknown, UNDEFINED => [ ],
        channels_mono, MONO => [
            FRONT_CENTER
        ],
        channels_mono_lfe, MONO_LFE => [
            FRONT_CENTER,
            LOW_FREQUENCY
        ],
        channels_stereo, STEREO => [
            FRONT_LEFT,
            FRONT_RIGHT
        ],
        channels_stereo_lfe, STEREO_LFE => [
            FRONT_LEFT,
            FRONT_RIGHT,
            LOW_FREQUENCY
        ],
        channels_3f, _3F => [
            FRONT_LEFT,
            FRONT_RIGHT,
            FRONT_CENTER
        ],
        channels_3f_lfe, _3F_LFE => [
            FRONT_LEFT,
            FRONT_RIGHT,
            FRONT_CENTER,
            LOW_FREQUENCY
        ],
        channels_2f1, _2F1 => [
            FRONT_LEFT,
            FRONT_RIGHT,
            BACK_CENTER
        ],
        channels_2f1_lfe, _2F1_LFE => [
            FRONT_LEFT,
            FRONT_RIGHT,
            LOW_FREQUENCY,
            BACK_CENTER
        ],
        channels_3f1, _3F1 => [
            FRONT_LEFT,
            FRONT_RIGHT,
            FRONT_CENTER,
            BACK_CENTER
        ],
        channels_3f1_lfe, _3F1_LFE => [
            FRONT_LEFT,
            FRONT_RIGHT,
            FRONT_CENTER,
            LOW_FREQUENCY,
            BACK_CENTER
        ],
        channels_2f2, _2F2 => [
            FRONT_LEFT,
            FRONT_RIGHT,
            SIDE_LEFT,
            SIDE_RIGHT
        ],
        channels_2f2_lfe, _2F2_LFE => [
            FRONT_LEFT,
            FRONT_RIGHT,
            LOW_FREQUENCY,
            SIDE_LEFT,
            SIDE_RIGHT
        ],
        channels_quad, QUAD => [
            FRONT_LEFT,
            FRONT_RIGHT,
            BACK_LEFT,
            BACK_RIGHT
        ],
        channels_quad_lfe, QUAD_LFE => [
            FRONT_LEFT,
            FRONT_RIGHT,
            LOW_FREQUENCY,
            BACK_LEFT,
            BACK_RIGHT
        ],
        channels_3f2, _3F2 => [
            FRONT_LEFT,
            FRONT_RIGHT,
            FRONT_CENTER,
            SIDE_LEFT,
            SIDE_RIGHT
        ],
        channels_3f2_lfe, _3F2_LFE => [
            FRONT_LEFT,
            FRONT_RIGHT,
            FRONT_CENTER,
            LOW_FREQUENCY,
            SIDE_LEFT,
            SIDE_RIGHT
        ],
        channels_3f2_back, _3F2_BACK => [
            FRONT_LEFT,
            FRONT_RIGHT,
            FRONT_CENTER,
            BACK_LEFT,
            BACK_RIGHT
        ],
        channels_3f2_lfe_back, _3F2_LFE_BACK => [
            FRONT_LEFT,
            FRONT_RIGHT,
            FRONT_CENTER,
            LOW_FREQUENCY,
            BACK_LEFT,
            BACK_RIGHT
        ],
        channels_3f3r_lfe, _3F3R_LFE => [
            FRONT_LEFT,
            FRONT_RIGHT,
            FRONT_CENTER,
            LOW_FREQUENCY,
            BACK_CENTER,
            SIDE_LEFT,
            SIDE_RIGHT
        ],
        channels_3f4_lfe, _3F4_LFE => [
            FRONT_LEFT,
            FRONT_RIGHT,
            FRONT_CENTER,
            LOW_FREQUENCY,
            BACK_LEFT,
            BACK_RIGHT,
            SIDE_LEFT,
            SIDE_RIGHT
        ]
    }

    #[test]
    fn mono_channels_enumerate() {
        let layout = ChannelLayout::MONO;
        let mut iter = super::channel_layout_iter(layout).enumerate();
        assert_eq!(Some((0, ChannelLayout::FRONT_CENTER)), iter.next());
        assert_eq!(None, iter.next());
    }

    #[test]
    fn stereo_channels_enumerate() {
        let layout = ChannelLayout::STEREO;
        let mut iter = super::channel_layout_iter(layout).enumerate();
        assert_eq!(Some((0, ChannelLayout::FRONT_LEFT)), iter.next());
        assert_eq!(Some((1, ChannelLayout::FRONT_RIGHT)), iter.next());
        assert_eq!(None, iter.next());
    }

    #[test]
    fn quad_channels_enumerate() {
        let layout = ChannelLayout::QUAD;
        let mut iter = super::channel_layout_iter(layout).enumerate();
        assert_eq!(Some((0, ChannelLayout::FRONT_LEFT)), iter.next());
        assert_eq!(Some((1, ChannelLayout::FRONT_RIGHT)), iter.next());
        assert_eq!(Some((2, ChannelLayout::BACK_LEFT)), iter.next());
        assert_eq!(Some((3, ChannelLayout::BACK_RIGHT)), iter.next());
        assert_eq!(None, iter.next());
    }

    macro_rules! map_channel_tests {
        {$($name: ident, $layout: ident => [ $($channels: ident),* ]),+} => {
            $(
            #[test]
            fn $name() {
                let map = layout_to_channel_map(ChannelLayout::$layout);
                assert_eq!(map.channels, map_channel_tests!(__COUNT__ $($channels)*));
                map_channel_tests!(__EACH__ map, 0, $($channels)*);
            }

            )*
        };
        (__COUNT__) => (0u8);
        (__COUNT__ $x:ident $($xs: ident)*) => (1u8 + map_channel_tests!(__COUNT__ $($xs)*));
        (__EACH__ $map:expr, $i:expr, ) => {};
        (__EACH__ $map:expr, $i:expr, $x:ident $($xs: ident)*) => {
            assert_eq!($map.map[$i], $x);
            map_channel_tests!(__EACH__ $map, $i+1, $($xs)* );
        };
    }

    map_channel_tests! {
        map_channel_mono, MONO => [
            PA_CHANNEL_POSITION_FRONT_CENTER
        ],
        map_channel_mono_lfe, MONO_LFE => [
            PA_CHANNEL_POSITION_FRONT_CENTER,
            PA_CHANNEL_POSITION_LFE
        ],
        map_channel_stereo, STEREO => [
            PA_CHANNEL_POSITION_FRONT_LEFT,
            PA_CHANNEL_POSITION_FRONT_RIGHT
        ],
        map_channel_stereo_lfe, STEREO_LFE => [
            PA_CHANNEL_POSITION_FRONT_LEFT,
            PA_CHANNEL_POSITION_FRONT_RIGHT,
            PA_CHANNEL_POSITION_LFE
        ],
        map_channel_3f, _3F => [
            PA_CHANNEL_POSITION_FRONT_LEFT,
            PA_CHANNEL_POSITION_FRONT_RIGHT,
            PA_CHANNEL_POSITION_FRONT_CENTER
        ],
        map_channel_3f_lfe, _3F_LFE => [
            PA_CHANNEL_POSITION_FRONT_LEFT,
            PA_CHANNEL_POSITION_FRONT_RIGHT,
            PA_CHANNEL_POSITION_FRONT_CENTER,
            PA_CHANNEL_POSITION_LFE
        ],
        map_channel_2f1, _2F1 => [
            PA_CHANNEL_POSITION_FRONT_LEFT,
            PA_CHANNEL_POSITION_FRONT_RIGHT,
            PA_CHANNEL_POSITION_REAR_CENTER
        ],
        map_channel_2f1_lfe, _2F1_LFE => [
            PA_CHANNEL_POSITION_FRONT_LEFT,
            PA_CHANNEL_POSITION_FRONT_RIGHT,
            PA_CHANNEL_POSITION_LFE,
            PA_CHANNEL_POSITION_REAR_CENTER
        ],
        map_channel_3f1, _3F1 => [
            PA_CHANNEL_POSITION_FRONT_LEFT,
            PA_CHANNEL_POSITION_FRONT_RIGHT,
            PA_CHANNEL_POSITION_FRONT_CENTER,
            PA_CHANNEL_POSITION_REAR_CENTER
        ],
        map_channel_3f1_lfe, _3F1_LFE =>[
            PA_CHANNEL_POSITION_FRONT_LEFT,
            PA_CHANNEL_POSITION_FRONT_RIGHT,
            PA_CHANNEL_POSITION_FRONT_CENTER,
            PA_CHANNEL_POSITION_LFE,
            PA_CHANNEL_POSITION_REAR_CENTER
        ],
        map_channel_2f2, _2F2 => [
            PA_CHANNEL_POSITION_FRONT_LEFT,
            PA_CHANNEL_POSITION_FRONT_RIGHT,
            PA_CHANNEL_POSITION_SIDE_LEFT,
            PA_CHANNEL_POSITION_SIDE_RIGHT
        ],
        map_channel_2f2_lfe, _2F2_LFE => [
            PA_CHANNEL_POSITION_FRONT_LEFT,
            PA_CHANNEL_POSITION_FRONT_RIGHT,
            PA_CHANNEL_POSITION_LFE,
            PA_CHANNEL_POSITION_SIDE_LEFT,
            PA_CHANNEL_POSITION_SIDE_RIGHT
        ],
        map_channel_quad, QUAD => [
            PA_CHANNEL_POSITION_FRONT_LEFT,
            PA_CHANNEL_POSITION_FRONT_RIGHT,
            PA_CHANNEL_POSITION_REAR_LEFT,
            PA_CHANNEL_POSITION_REAR_RIGHT
        ],
        map_channel_quad_lfe, QUAD_LFE => [
            PA_CHANNEL_POSITION_FRONT_LEFT,
            PA_CHANNEL_POSITION_FRONT_RIGHT,
            PA_CHANNEL_POSITION_LFE,
            PA_CHANNEL_POSITION_REAR_LEFT,
            PA_CHANNEL_POSITION_REAR_RIGHT
        ],
        map_channel_3f2, _3F2 => [
            PA_CHANNEL_POSITION_FRONT_LEFT,
            PA_CHANNEL_POSITION_FRONT_RIGHT,
            PA_CHANNEL_POSITION_FRONT_CENTER,
            PA_CHANNEL_POSITION_SIDE_LEFT,
            PA_CHANNEL_POSITION_SIDE_RIGHT
        ],
        map_channel_3f2_lfe, _3F2_LFE => [
            PA_CHANNEL_POSITION_FRONT_LEFT,
            PA_CHANNEL_POSITION_FRONT_RIGHT,
            PA_CHANNEL_POSITION_FRONT_CENTER,
            PA_CHANNEL_POSITION_LFE,
            PA_CHANNEL_POSITION_SIDE_LEFT,
            PA_CHANNEL_POSITION_SIDE_RIGHT
        ],
        map_channel_3f2_back, _3F2_BACK => [
            PA_CHANNEL_POSITION_FRONT_LEFT,
            PA_CHANNEL_POSITION_FRONT_RIGHT,
            PA_CHANNEL_POSITION_FRONT_CENTER,
            PA_CHANNEL_POSITION_REAR_LEFT,
            PA_CHANNEL_POSITION_REAR_RIGHT
        ],
        map_channel_3f2_lfe_back, _3F2_LFE_BACK => [
            PA_CHANNEL_POSITION_FRONT_LEFT,
            PA_CHANNEL_POSITION_FRONT_RIGHT,
            PA_CHANNEL_POSITION_FRONT_CENTER,
            PA_CHANNEL_POSITION_LFE,
            PA_CHANNEL_POSITION_REAR_LEFT,
            PA_CHANNEL_POSITION_REAR_RIGHT
        ],
        map_channel_3f3r_lfe, _3F3R_LFE => [
            PA_CHANNEL_POSITION_FRONT_LEFT,
            PA_CHANNEL_POSITION_FRONT_RIGHT,
            PA_CHANNEL_POSITION_FRONT_CENTER,
            PA_CHANNEL_POSITION_LFE,
            PA_CHANNEL_POSITION_REAR_CENTER,
            PA_CHANNEL_POSITION_SIDE_LEFT,
            PA_CHANNEL_POSITION_SIDE_RIGHT
        ],
        map_channel_3f4_lfe, _3F4_LFE => [
            PA_CHANNEL_POSITION_FRONT_LEFT,
            PA_CHANNEL_POSITION_FRONT_RIGHT,
            PA_CHANNEL_POSITION_FRONT_CENTER,
            PA_CHANNEL_POSITION_LFE,
            PA_CHANNEL_POSITION_REAR_LEFT,
            PA_CHANNEL_POSITION_REAR_RIGHT,
            PA_CHANNEL_POSITION_SIDE_LEFT,
            PA_CHANNEL_POSITION_SIDE_RIGHT
        ]
    }
}
