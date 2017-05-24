// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use backend::*;
use backend::cork_state::CorkState;
use cubeb;
use pulse_ffi::*;
use std::os::raw::{c_char, c_long, c_void};
use std::ptr;

const PULSE_NO_GAIN: f32 = -1.0;

fn cubeb_channel_to_pa_channel(channel: cubeb::Channel) -> pa_channel_position_t {
    assert_ne!(channel, cubeb::CHANNEL_INVALID);

    // This variable may be used for multiple times, so we should avoid to
    // allocate it in stack, or it will be created and removed repeatedly.
    // Use static to allocate this local variable in data space instead of stack.
    static MAP: [pa_channel_position_t; 10] = [
        // PA_CHANNEL_POSITION_INVALID,      // CHANNEL_INVALID
        PA_CHANNEL_POSITION_MONO,         // CHANNEL_MONO
        PA_CHANNEL_POSITION_FRONT_LEFT,   // CHANNEL_LEFT
        PA_CHANNEL_POSITION_FRONT_RIGHT,  // CHANNEL_RIGHT
        PA_CHANNEL_POSITION_FRONT_CENTER, // CHANNEL_CENTER
        PA_CHANNEL_POSITION_SIDE_LEFT,    // CHANNEL_LS
        PA_CHANNEL_POSITION_SIDE_RIGHT,   // CHANNEL_RS
        PA_CHANNEL_POSITION_REAR_LEFT,    // CHANNEL_RLS
        PA_CHANNEL_POSITION_REAR_CENTER,  // CHANNEL_RCENTER
        PA_CHANNEL_POSITION_REAR_RIGHT,   // CHANNEL_RRS
        PA_CHANNEL_POSITION_LFE           // CHANNEL_LFE
    ];

    let idx: i32 = channel.into();
    MAP[idx as usize]
}

fn layout_to_channel_map(layout: cubeb::ChannelLayout) -> pa_channel_map {
    assert_ne!(layout, cubeb::LAYOUT_UNDEFINED);

    let order = cubeb::mixer::channel_index_to_order(layout);

    let mut cm: pa_channel_map = Default::default();
    unsafe {
        pa_channel_map_init(&mut cm);
    }
    cm.channels = order.len() as u8;
    for (s, d) in order.iter().zip(cm.map.iter_mut()) {
        *d = cubeb_channel_to_pa_channel(*s);
    }
    cm
}

pub struct Device(cubeb::Device);

impl Drop for Device {
    fn drop(&mut self) {
        unsafe {
            pa_xfree(self.0.input_name as *mut _);
            pa_xfree(self.0.output_name as *mut _);
        }
    }
}


#[derive(Debug)]
pub struct Stream<'ctx> {
    context: &'ctx Context,
    output_stream: *mut pa_stream,
    input_stream: *mut pa_stream,
    data_callback: cubeb::DataCallback,
    state_callback: cubeb::StateCallback,
    user_ptr: *mut c_void,
    drain_timer: *mut pa_time_event,
    output_sample_spec: pa_sample_spec,
    input_sample_spec: pa_sample_spec,
    shutdown: bool,
    volume: f32,
    state: cubeb::State,
}

impl<'ctx> Drop for Stream<'ctx> {
    fn drop(&mut self) {
        self.destroy();
    }
}

impl<'ctx> Stream<'ctx> {
    pub fn new(context: &'ctx Context,
               stream_name: *const c_char,
               input_device: cubeb::DeviceId,
               input_stream_params: Option<cubeb::StreamParams>,
               output_device: cubeb::DeviceId,
               output_stream_params: Option<cubeb::StreamParams>,
               latency_frames: u32,
               data_callback: cubeb::DataCallback,
               state_callback: cubeb::StateCallback,
               user_ptr: *mut c_void)
               -> Result<Box<Stream<'ctx>>> {

        let mut stm = Box::new(Stream {
                                   context: context,
                                   output_stream: ptr::null_mut(),
                                   input_stream: ptr::null_mut(),
                                   data_callback: data_callback,
                                   state_callback: state_callback,
                                   user_ptr: user_ptr,
                                   drain_timer: ptr::null_mut(),
                                   output_sample_spec: pa_sample_spec::default(),
                                   input_sample_spec: pa_sample_spec::default(),
                                   shutdown: false,
                                   volume: PULSE_NO_GAIN,
                                   state: cubeb::STATE_ERROR,
                               });

        unsafe {
            pa_threaded_mainloop_lock(stm.context.mainloop);
            if let Some(ref stream_params) = output_stream_params {
                match stm.pulse_stream_init(stream_params, stream_name) {
                    Ok(s) => stm.output_stream = s,
                    Err(e) => {
                        pa_threaded_mainloop_unlock(stm.context.mainloop);
                        stm.destroy();
                        return Err(e);
                    },
                }

                stm.output_sample_spec = *pa_stream_get_sample_spec(stm.output_stream);

                pa_stream_set_state_callback(stm.output_stream,
                                             Some(stream_state_callback),
                                             stm.as_mut() as *mut _ as *mut _);
                pa_stream_set_write_callback(stm.output_stream,
                                             Some(stream_write_callback),
                                             stm.as_mut() as *mut _ as *mut _);

                let battr = set_buffering_attribute(latency_frames, &stm.output_sample_spec);
                pa_stream_connect_playback(stm.output_stream,
                                           output_device as *mut c_char,
                                           &battr,
                                           PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_INTERPOLATE_TIMING |
                                           PA_STREAM_START_CORKED |
                                           PA_STREAM_ADJUST_LATENCY,
                                           ptr::null(),
                                           ptr::null_mut());
            }

            // Set up input stream
            if let Some(ref stream_params) = input_stream_params {
                match stm.pulse_stream_init(stream_params, stream_name) {
                    Ok(s) => stm.input_stream = s,
                    Err(e) => {
                        pa_threaded_mainloop_unlock(stm.context.mainloop);
                        stm.destroy();
                        return Err(e);
                    },
                }

                stm.input_sample_spec = *(pa_stream_get_sample_spec(stm.input_stream));

                pa_stream_set_state_callback(stm.input_stream,
                                             Some(stream_state_callback),
                                             stm.as_mut() as *mut _ as *mut _);
                pa_stream_set_read_callback(stm.input_stream,
                                            Some(stream_read_callback),
                                            stm.as_mut() as *mut _ as *mut _);

                let battr = set_buffering_attribute(latency_frames, &stm.input_sample_spec);
                pa_stream_connect_record(stm.input_stream,
                                         input_device as *mut c_char,
                                         &battr,
                                         PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_INTERPOLATE_TIMING |
                                         PA_STREAM_START_CORKED |
                                         PA_STREAM_ADJUST_LATENCY);
            }

            let r = if stm.wait_until_stream_ready() {
                /* force a timing update now, otherwise timing info does not become valid
                until some point after initialization has completed. */
                stm.update_timing_info()
            } else {
                false
            };

            pa_threaded_mainloop_unlock(stm.context.mainloop);

            if !r {
                stm.destroy();
                return Err(cubeb::ERROR);
            }

            if cubeb::log_enabled() {
                if output_stream_params.is_some() {
                    let output_att = *pa_stream_get_buffer_attr(stm.output_stream);
                    log!("Output buffer attributes maxlength %u, tlength %u, \
                         prebuf %u, minreq %u, fragsize %u",
                         output_att.maxlength,
                         output_att.tlength,
                         output_att.prebuf,
                         output_att.minreq,
                         output_att.fragsize);
                }

                if input_stream_params.is_some() {
                    let input_att = *pa_stream_get_buffer_attr(stm.input_stream);
                    log!("Input buffer attributes maxlength %u, tlength %u, \
                          prebuf %u, minreq %u, fragsize %u",
                         input_att.maxlength,
                         input_att.tlength,
                         input_att.prebuf,
                         input_att.minreq,
                         input_att.fragsize);
                }
            }
        }

        Ok(stm)
    }

    fn destroy(&mut self) {
        self.stream_cork(CorkState::cork());

        unsafe {
            pa_threaded_mainloop_lock(self.context.mainloop);
            if !self.output_stream.is_null() {
                if !self.drain_timer.is_null() {
                    /* there's no pa_rttime_free, so use this instead. */
                    let ma = pa_threaded_mainloop_get_api(self.context.mainloop);
                    if !ma.is_null() {
                        (*ma).time_free.unwrap()(self.drain_timer);
                    }
                }

                pa_stream_set_state_callback(self.output_stream, None, ptr::null_mut());
                pa_stream_set_write_callback(self.output_stream, None, ptr::null_mut());
                pa_stream_disconnect(self.output_stream);
                pa_stream_unref(self.output_stream);
            }

            if !self.input_stream.is_null() {
                pa_stream_set_state_callback(self.input_stream, None, ptr::null_mut());
                pa_stream_set_read_callback(self.input_stream, None, ptr::null_mut());
                pa_stream_disconnect(self.input_stream);
                pa_stream_unref(self.input_stream);
            }
            pa_threaded_mainloop_unlock(self.context.mainloop);
        }
    }

    pub fn start(&mut self) -> i32 {
        self.shutdown = false;
        self.stream_cork(CorkState::uncork() | CorkState::notify());

        if !self.output_stream.is_null() && self.input_stream.is_null() {
            unsafe {
                /* On output only case need to manually call user cb once in order to make
                 * things roll. This is done via a defer event in order to execute it
                 * from PA server thread. */
                pa_threaded_mainloop_lock(self.context.mainloop);
                pa_mainloop_api_once(pa_threaded_mainloop_get_api(self.context.mainloop),
                                     Some(pulse_defer_event_cb),
                                     self as *mut _ as *mut _);
                pa_threaded_mainloop_unlock(self.context.mainloop);
            }
        }

        cubeb::OK
    }

    pub fn stop(&mut self) -> i32 {
        unsafe {
            pa_threaded_mainloop_lock(self.context.mainloop);
            self.shutdown = true;
            // If draining is taking place wait to finish
            while !self.drain_timer.is_null() {
                pa_threaded_mainloop_wait(self.context.mainloop);
            }
            pa_threaded_mainloop_unlock(self.context.mainloop);
        }
        self.stream_cork(CorkState::cork() | CorkState::notify());

        cubeb::OK
    }

    pub fn position(&self) -> Result<u64> {
        if self.output_stream.is_null() {
            return Err(cubeb::ERROR);
        }

        let position = unsafe {
            let in_thread = pa_threaded_mainloop_in_thread(self.context.mainloop);

            if in_thread == 0 {
                pa_threaded_mainloop_lock(self.context.mainloop);
            }

            let mut r_usec: pa_usec_t = Default::default();
            let r = pa_stream_get_time(self.output_stream, &mut r_usec);
            if in_thread == 0 {
                pa_threaded_mainloop_unlock(self.context.mainloop);
            }

            if r != 0 {
                return Err(cubeb::ERROR);
            }

            let bytes = pa_usec_to_bytes(r_usec, &self.output_sample_spec);
            (bytes / pa_frame_size(&self.output_sample_spec)) as u64
        };
        Ok(position)
    }

    pub fn latency(&self) -> Result<u32> {
        if self.output_stream.is_null() {
            return Err(cubeb::ERROR);
        }

        let mut r_usec: pa_usec_t = 0;
        let mut negative: i32 = 0;
        let r = unsafe { pa_stream_get_latency(self.output_stream, &mut r_usec, &mut negative) };

        if r != 0 {
            return Err(cubeb::ERROR);
        }

        debug_assert_eq!(negative, 0);
        let latency = (r_usec * self.output_sample_spec.rate as pa_usec_t / PA_USEC_PER_SEC) as u32;

        Ok(latency)
    }

    pub fn set_volume(&mut self, volume: f32) -> i32 {
        if self.output_stream.is_null() {
            return cubeb::ERROR;
        }

        unsafe {
            pa_threaded_mainloop_lock(self.context.mainloop);

            while self.context.default_sink_info.is_none() {
                pa_threaded_mainloop_wait(self.context.mainloop);
            }

            let mut cvol: pa_cvolume = Default::default();

            /* if the pulse daemon is configured to use flat volumes,
             * apply our own gain instead of changing the input volume on the sink. */
            let flags = {
                match self.context.default_sink_info {
                    Some(ref info) => info.flags,
                    _ => 0,
                }
            };

            if (flags & PA_SINK_FLAT_VOLUME) != 0 {
                self.volume = volume;
            } else {
                let ss = pa_stream_get_sample_spec(self.output_stream);
                let vol = pa_sw_volume_from_linear(volume as f64);
                pa_cvolume_set(&mut cvol, (*ss).channels as u32, vol);

                let index = pa_stream_get_index(self.output_stream);

                let op = pa_context_set_sink_input_volume(self.context.context,
                                                          index,
                                                          &cvol,
                                                          Some(volume_success),
                                                          self as *mut _ as *mut _);
                if !op.is_null() {
                    self.context.operation_wait(self.output_stream, op);
                    pa_operation_unref(op);
                }
            }

            pa_threaded_mainloop_unlock(self.context.mainloop);
        }
        cubeb::OK
    }

    pub fn set_panning(&mut self, panning: f32) -> i32 {
        if self.output_stream.is_null() {
            return cubeb::ERROR;
        }

        unsafe {
            pa_threaded_mainloop_lock(self.context.mainloop);

            let map = pa_stream_get_channel_map(self.output_stream);
            if pa_channel_map_can_balance(map) == 0 {
                pa_threaded_mainloop_unlock(self.context.mainloop);
                return cubeb::ERROR;
            }

            let index = pa_stream_get_index(self.output_stream);

            let mut cvol: pa_cvolume = Default::default();
            let mut r = SinkInputInfoResult {
                cvol: &mut cvol,
                mainloop: self.context.mainloop,
            };

            let op = pa_context_get_sink_input_info(self.context.context,
                                                    index,
                                                    Some(sink_input_info_cb),
                                                    &mut r as *mut _ as *mut _);
            if !op.is_null() {
                self.context.operation_wait(self.output_stream, op);
                pa_operation_unref(op);
            }

            pa_cvolume_set_balance(&mut cvol, map, panning);

            let op = pa_context_set_sink_input_volume(self.context.context,
                                                      index,
                                                      &cvol,
                                                      Some(volume_success),
                                                      self as *mut _ as *mut _);
            if !op.is_null() {
                self.context.operation_wait(self.output_stream, op);
                pa_operation_unref(op);
            }

            pa_threaded_mainloop_unlock(self.context.mainloop);
        }

        cubeb::OK
    }

    pub fn current_device(&self) -> Result<Box<cubeb::Device>> {
        if self.context.version_0_9_8 {
            let mut dev = Box::new(cubeb::Device::default());

            if !self.input_stream.is_null() {
                dev.input_name = unsafe { pa_xstrdup(pa_stream_get_device_name(self.input_stream)) };
            }

            if !self.output_stream.is_null() {
                dev.output_name = unsafe { pa_xstrdup(pa_stream_get_device_name(self.output_stream)) };
            }

            Ok(dev)
        } else {
            Err(cubeb::ERROR_NOT_SUPPORTED)
        }
    }

    fn pulse_stream_init(&mut self,
                         stream_params: &cubeb::StreamParams,
                         stream_name: *const c_char)
                         -> Result<*mut pa_stream> {

        fn to_pulse_format(format: cubeb::SampleFormat) -> pa_sample_format_t {
            match format {
                cubeb::SAMPLE_S16LE => PA_SAMPLE_S16LE,
                cubeb::SAMPLE_S16BE => PA_SAMPLE_S16BE,
                cubeb::SAMPLE_FLOAT32LE => PA_SAMPLE_FLOAT32LE,
                cubeb::SAMPLE_FLOAT32BE => PA_SAMPLE_FLOAT32BE,
                _ => panic!("Invalid format: {:?}", format),
            }
        }

        let fmt = to_pulse_format(stream_params.format);
        if fmt == PA_SAMPLE_INVALID {
            return Err(cubeb::ERROR_INVALID_FORMAT);
        }

        let ss = pa_sample_spec {
            channels: stream_params.channels as u8,
            format: fmt,
            rate: stream_params.rate,
        };

        let stream = if stream_params.layout == cubeb::LAYOUT_UNDEFINED {
            unsafe { pa_stream_new(self.context.context, stream_name, &ss, ptr::null_mut()) }
        } else {
            let cm = layout_to_channel_map(stream_params.layout);
            unsafe { pa_stream_new(self.context.context, stream_name, &ss, &cm) }
        };

        if !stream.is_null() {
            Ok(stream)
        } else {
            Err(cubeb::ERROR)
        }
    }

    fn stream_cork(&mut self, state: CorkState) {
        unsafe { pa_threaded_mainloop_lock(self.context.mainloop) };
        self.context.pulse_stream_cork(self.output_stream, state);
        self.context.pulse_stream_cork(self.input_stream, state);
        unsafe { pa_threaded_mainloop_unlock(self.context.mainloop) };

        if state.is_notify() {
            self.state_change_callback(if state.is_cork() {
                                           cubeb::STATE_STOPPED
                                       } else {
                                           cubeb::STATE_STARTED
                                       });
        }
    }

    fn update_timing_info(&self) -> bool {
        let mut r = false;

        if !self.output_stream.is_null() {
            let o = unsafe {
                pa_stream_update_timing_info(self.output_stream,
                                             Some(stream_success_callback),
                                             self as *const _ as *mut _)
            };

            if !o.is_null() {
                r = self.context.operation_wait(self.output_stream, o);
                unsafe {
                    pa_operation_unref(o);
                }
            }

            if !r {
                return r;
            }
        }

        if !self.input_stream.is_null() {
            let o = unsafe {
                pa_stream_update_timing_info(self.input_stream,
                                             Some(stream_success_callback),
                                             self as *const _ as *mut _)
            };

            if !o.is_null() {
                r = self.context.operation_wait(self.input_stream, o);
                unsafe {
                    pa_operation_unref(o);
                }
            }
        }

        r
    }

    pub fn state_change_callback(&mut self, s: cubeb::State) {
        self.state = s;
        unsafe {
            (self.state_callback.unwrap())(self as *mut Stream as *mut cubeb::Stream, self.user_ptr, s);
        }
    }

    fn wait_until_stream_ready(&self) -> bool {
        if !self.output_stream.is_null() && !wait_until_io_stream_ready(self.output_stream, self.context.mainloop) {
            return false;
        }

        if !self.input_stream.is_null() && !wait_until_io_stream_ready(self.input_stream, self.context.mainloop) {
            return false;
        }

        true
    }

    fn trigger_user_callback(&mut self, s: *mut pa_stream, input_data: *const c_void, nbytes: usize) {
        let frame_size = unsafe { pa_frame_size(&self.output_sample_spec) };
        debug_assert_eq!(nbytes % frame_size, 0);

        let mut buffer: *mut c_void = ptr::null_mut();
        let mut r: i32;

        let mut towrite = nbytes;
        let mut read_offset = 0usize;
        while towrite > 0 {
            let mut size = towrite;
            r = unsafe { pa_stream_begin_write(s, &mut buffer, &mut size) };
            // Note: this has failed running under rr on occassion - needs investigation.
            debug_assert_eq!(r, 0);
            debug_assert!(size > 0);
            debug_assert_eq!(size % frame_size, 0);

            logv!("Trigger user callback with output buffer size={}, read_offset={}",
                  size,
                  read_offset);
            let read_ptr = unsafe { (input_data as *const u8).offset(read_offset as isize) };
            let got = unsafe {
                self.data_callback.unwrap()(self as *const _ as *mut _,
                                            self.user_ptr,
                                            read_ptr as *const _ as *mut _,
                                            buffer,
                                            (size / frame_size) as c_long)
            };
            if got < 0 {
                unsafe {
                    pa_stream_cancel_write(s);
                }
                self.shutdown = true;
                return;
            }
            // If more iterations move offset of read buffer
            if !input_data.is_null() {
                let in_frame_size = unsafe { pa_frame_size(&self.input_sample_spec) };
                read_offset += (size / frame_size) * in_frame_size;
            }

            if self.volume != PULSE_NO_GAIN {
                let samples = (self.output_sample_spec.channels as usize * size / frame_size) as isize;

                if self.output_sample_spec.format == PA_SAMPLE_S16BE ||
                   self.output_sample_spec.format == PA_SAMPLE_S16LE {
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

            r = unsafe {
                pa_stream_write(s,
                                buffer,
                                got as usize * frame_size,
                                None,
                                0,
                                PA_SEEK_RELATIVE)
            };
            debug_assert_eq!(r, 0);

            if (got as usize) < size / frame_size {
                let mut latency: pa_usec_t = 0;
                let rr: i32 = unsafe { pa_stream_get_latency(s, &mut latency, ptr::null_mut()) };
                if rr == -(PA_ERR_NODATA as i32) {
                    /* this needs a better guess. */
                    latency = 100 * PA_USEC_PER_MSEC;
                }
                debug_assert!(r == 0 || r == -(PA_ERR_NODATA as i32));
                /* pa_stream_drain is useless, see PA bug# 866. this is a workaround. */
                /* arbitrary safety margin: double the current latency. */
                debug_assert!(self.drain_timer.is_null());
                self.drain_timer = unsafe {
                    pa_context_rttime_new(self.context.context,
                                          pa_rtclock_now() + 2 * latency,
                                          Some(stream_drain_callback),
                                          self as *const _ as *mut _)
                };
                self.shutdown = true;
                return;
            }

            towrite -= size;
        }

        debug_assert_eq!(towrite, 0);
    }
}

unsafe extern "C" fn stream_success_callback(_s: *mut pa_stream, _success: i32, u: *mut c_void) {
    let stm = &*(u as *mut Stream);
    pa_threaded_mainloop_signal(stm.context.mainloop, 0);
}

unsafe extern "C" fn stream_drain_callback(a: *mut pa_mainloop_api,
                                           e: *mut pa_time_event,
                                           _tv: *const timeval,
                                           u: *mut c_void) {
    let mut stm = &mut *(u as *mut Stream);
    debug_assert_eq!(stm.drain_timer, e);
    stm.state_change_callback(cubeb::STATE_DRAINED);
    /* there's no pa_rttime_free, so use this instead. */
    (*a).time_free.unwrap()(stm.drain_timer);
    stm.drain_timer = ptr::null_mut();
    pa_threaded_mainloop_signal(stm.context.mainloop, 0);
}

unsafe extern "C" fn stream_state_callback(s: *mut pa_stream, u: *mut c_void) {
    let stm = &mut *(u as *mut Stream);
    if !PA_STREAM_IS_GOOD(pa_stream_get_state(s)) {
        stm.state_change_callback(cubeb::STATE_ERROR);
    }
    pa_threaded_mainloop_signal(stm.context.mainloop, 0);
}

fn read_from_input(s: *mut pa_stream, buffer: *mut *const c_void, size: *mut usize) -> i32 {
    let readable_size = unsafe { pa_stream_readable_size(s) };
    if readable_size > 0 && unsafe { pa_stream_peek(s, buffer, size) } < 0 {
        return -1;
    }

    readable_size as i32
}

unsafe extern "C" fn stream_write_callback(s: *mut pa_stream, nbytes: usize, u: *mut c_void) {
    logv!("Output callback to be written buffer size {}", nbytes);
    let mut stm = &mut *(u as *mut Stream);
    if stm.shutdown || stm.state != cubeb::STATE_STARTED {
        return;
    }

    if stm.input_stream.is_null() {
        // Output/playback only operation.
        // Write directly to output
        debug_assert!(!stm.output_stream.is_null());
        stm.trigger_user_callback(s, ptr::null(), nbytes);
    }
}

unsafe extern "C" fn stream_read_callback(s: *mut pa_stream, nbytes: usize, u: *mut c_void) {
    logv!("Input callback buffer size {}", nbytes);
    let mut stm = &mut *(u as *mut Stream);
    if stm.shutdown {
        return;
    }

    let mut read_data: *const c_void = ptr::null();
    let mut read_size: usize = 0;
    while read_from_input(s, &mut read_data, &mut read_size) > 0 {
        /* read_data can be NULL in case of a hole. */
        if !read_data.is_null() {
            let in_frame_size = pa_frame_size(&stm.input_sample_spec);
            let read_frames = read_size / in_frame_size;

            if !stm.output_stream.is_null() {
                // input/capture + output/playback operation
                let out_frame_size = pa_frame_size(&stm.output_sample_spec);
                let write_size = read_frames * out_frame_size;
                // Offer full duplex data for writing
                let stream = stm.output_stream;
                stm.trigger_user_callback(stream, read_data, write_size);
            } else {
                // input/capture only operation. Call callback directly
                let got = stm.data_callback.unwrap()(stm as *mut _ as *mut _,
                                                     stm.user_ptr,
                                                     read_data,
                                                     ptr::null_mut(),
                                                     read_frames as c_long);
                if got < 0 || got as usize != read_frames {
                    pa_stream_cancel_write(s);
                    stm.shutdown = true;
                    break;
                }
            }
        }

        if read_size > 0 {
            pa_stream_drop(s);
        }

        if stm.shutdown {
            return;
        }
    }
}

fn wait_until_io_stream_ready(stream: *mut pa_stream, mainloop: *mut pa_threaded_mainloop) -> bool {
    if stream.is_null() || mainloop.is_null() {
        return false;
    }

    loop {
        let state = unsafe { pa_stream_get_state(stream) };
        if !PA_STREAM_IS_GOOD(state) {
            return false;
        }
        if state == PA_STREAM_READY {
            break;
        }
        unsafe { pa_threaded_mainloop_wait(mainloop) };
    }

    true
}

fn set_buffering_attribute(latency_frames: u32, sample_spec: &pa_sample_spec) -> pa_buffer_attr {
    let tlength = latency_frames * unsafe { pa_frame_size(sample_spec) } as u32;
    let minreq = tlength / 4;
    let battr = pa_buffer_attr {
        maxlength: u32::max_value(),
        prebuf: u32::max_value(),
        tlength: tlength,
        minreq: minreq,
        fragsize: minreq,
    };

    log!("Requested buffer attributes maxlength {}, tlength {}, prebuf {}, minreq {}, fragsize {}",
         battr.maxlength,
         battr.tlength,
         battr.prebuf,
         battr.minreq,
         battr.fragsize);

    battr
}

unsafe extern "C" fn pulse_defer_event_cb(_a: *mut pa_mainloop_api, u: *mut c_void) {
    let mut stm = &mut *(u as *mut Stream);
    if stm.shutdown {
        return;
    }
    let writable_size = pa_stream_writable_size(stm.output_stream);
    let stream = stm.output_stream;
    stm.trigger_user_callback(stream, ptr::null_mut(), writable_size);
}

#[repr(C)]
struct SinkInputInfoResult {
    pub cvol: *mut pa_cvolume,
    pub mainloop: *mut pa_threaded_mainloop,
}

unsafe extern "C" fn sink_input_info_cb(_c: *mut pa_context, i: *const pa_sink_input_info, eol: i32, u: *mut c_void) {
    let info = &*i;
    let mut r = &mut *(u as *mut SinkInputInfoResult);
    if eol == 0 {
        *r.cvol = info.volume;
    }
    pa_threaded_mainloop_signal(r.mainloop, 0);
}

unsafe extern "C" fn volume_success(_c: *mut pa_context, success: i32, u: *mut c_void) {
    let stm = &*(u as *mut Stream);
    debug_assert_ne!(success, 0);
    pa_threaded_mainloop_signal(stm.context.mainloop, 0);
}
