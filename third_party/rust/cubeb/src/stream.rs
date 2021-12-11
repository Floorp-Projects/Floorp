// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

//! Stream Functions
//!
//! # Example
//! ```no_run
//! extern crate cubeb;
//! use std::thread;
//! use std::time::Duration;
//!
//! type Frame = cubeb::MonoFrame<f32>;
//!
//! fn main() {
//!     let ctx = cubeb::init("Cubeb tone example").unwrap();
//!
//!     let params = cubeb::StreamParamsBuilder::new()
//!         .format(cubeb::SampleFormat::Float32LE)
//!         .rate(44_100)
//!         .channels(1)
//!         .layout(cubeb::ChannelLayout::MONO)
//!         .prefs(cubeb::StreamPrefs::NONE)
//!         .take();
//!
//!     let phase_inc = 440.0 / 44_100.0;
//!     let mut phase = 0.0;
//!     let volume = 0.25;
//!
//!     let mut builder = cubeb::StreamBuilder::<Frame>::new();
//!     builder
//!         .name("Cubeb Square Wave")
//!         .default_output(&params)
//!         .latency(0x1000)
//!         .data_callback(move |_, output| {
//!             // Generate a square wave
//!             for x in output.iter_mut() {
//!                 x.m = if phase < 0.5 { volume } else { -volume };
//!                 phase = (phase + phase_inc) % 1.0;
//!             }
//!
//!             output.len() as isize
//!         })
//!         .state_callback(|state| {
//!             println!("stream {:?}", state);
//!         });
//!     let stream = builder.init(&ctx).expect("Failed to create stream.");
//!
//!     // Start playback
//!     stream.start().unwrap();
//!
//!     // Play for 1/2 second
//!     thread::sleep(Duration::from_millis(500));
//!
//!     // Shutdown
//!     stream.stop().unwrap();
//! }
//! ```

use {ContextRef, DeviceId, Error, Result, State, StreamParamsRef};
use cubeb_core;
use ffi;
use std::{ops, panic, ptr};
use std::ffi::CString;
use std::marker::PhantomData;
use std::mem::ManuallyDrop;
use std::os::raw::{c_long, c_void};
use std::slice::{from_raw_parts, from_raw_parts_mut};

pub type DataCallback<F> = dyn FnMut(&[F], &mut [F]) -> isize + Send + Sync + 'static;
pub type StateCallback = dyn FnMut(State) + Send + Sync + 'static;
pub type DeviceChangedCallback = dyn FnMut() + Send + Sync + 'static;

pub struct StreamCallbacks<F> {
    pub(crate) data: Box<DataCallback<F>>,
    pub(crate) state: Box<StateCallback>,
    pub(crate) device_changed: Option<Box<DeviceChangedCallback>>,
}

pub struct Stream<F>(ManuallyDrop<cubeb_core::Stream>,
                     PhantomData<*const F>);

impl<F> Stream<F> {
    fn new(s: cubeb_core::Stream) -> Stream<F> {
        Stream(ManuallyDrop::new(s), PhantomData)
    }
}

impl<F> Drop for Stream<F> {
    fn drop(&mut self) {
        let user_ptr = self.user_ptr();
        unsafe { ManuallyDrop::drop(&mut self.0) };
        let _ = unsafe { Box::from_raw(user_ptr as *mut StreamCallbacks<F>) };
    }
}

impl<F> ops::Deref for Stream<F> {
    type Target = cubeb_core::Stream;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

pub struct StreamBuilder<'a, F> {
    name: Option<CString>,
    input: Option<(DeviceId, &'a StreamParamsRef)>,
    output: Option<(DeviceId, &'a StreamParamsRef)>,
    latency: Option<u32>,
    data_cb: Option<Box<DataCallback<F>>>,
    state_cb: Option<Box<StateCallback>>,
    device_changed_cb: Option<Box<DeviceChangedCallback>>,
}

impl<'a, F> StreamBuilder<'a, F> {
    pub fn new() -> StreamBuilder<'a, F> {
        StreamBuilder {
            name: None,
            input: None,
            output: None,
            latency: None,
            data_cb: None,
            state_cb: None,
            device_changed_cb: None,
        }
    }

    pub fn data_callback<D>(&mut self, cb: D) -> &mut Self
    where
        D: FnMut(&[F], &mut [F]) -> isize + Send + Sync + 'static,
    {
        self.data_cb = Some(Box::new(cb) as Box<DataCallback<F>>);
        self
    }
    pub fn state_callback<S>(&mut self, cb: S) -> &mut Self
    where
        S: FnMut(State) + Send + Sync + 'static,
    {
        self.state_cb = Some(Box::new(cb) as Box<StateCallback>);
        self
    }

    pub fn name<T: Into<Vec<u8>>>(&mut self, name: T) -> &mut Self {
        self.name = Some(CString::new(name).unwrap());
        self
    }

    pub fn default_input(&mut self, params: &'a StreamParamsRef) -> &mut Self {
        self.input = Some((ptr::null(), params));
        self
    }

    pub fn input(&mut self, device: DeviceId, params: &'a StreamParamsRef) -> &mut Self {
        self.input = Some((device, params));
        self
    }

    pub fn default_output(&mut self, params: &'a StreamParamsRef) -> &mut Self {
        self.output = Some((ptr::null(), params));
        self
    }

    pub fn output(&mut self, device: DeviceId, params: &'a StreamParamsRef) -> &mut Self {
        self.output = Some((device, params));
        self
    }

    pub fn latency(&mut self, latency: u32) -> &mut Self {
        self.latency = Some(latency);
        self
    }

    pub fn device_changed_cb<CB>(&mut self, cb: CB) -> &mut Self
    where
        CB: FnMut() + Send + Sync + 'static,
    {
        self.device_changed_cb = Some(Box::new(cb) as Box<DeviceChangedCallback>);
        self
    }

    pub fn init(self, ctx: &ContextRef) -> Result<Stream<F>> {
        if self.data_cb.is_none() || self.state_cb.is_none() {
            return Err(Error::error());
        }

        let has_device_changed = self.device_changed_cb.is_some();
        let cbs = Box::into_raw(Box::new(StreamCallbacks {
            data: self.data_cb.unwrap(),
            state: self.state_cb.unwrap(),
            device_changed: self.device_changed_cb,
        }));

        let stream_name = self.name.as_ref().and_then(|n| Some(n.as_c_str()));
        let (input_device, input_stream_params) =
            self.input.map_or((ptr::null(), None), |x| (x.0, Some(x.1)));
        let (output_device, output_stream_params) = self.output
            .map_or((ptr::null(), None), |x| (x.0, Some(x.1)));
        let latency = self.latency.unwrap_or(1);
        let data_callback: ffi::cubeb_data_callback = Some(data_cb_c::<F>);
        let state_callback: ffi::cubeb_state_callback = Some(state_cb_c::<F>);

        let stream = unsafe {
            ctx.stream_init(
                stream_name,
                input_device,
                input_stream_params,
                output_device,
                output_stream_params,
                latency,
                data_callback,
                state_callback,
                cbs as *mut _
            )?
        };
        if has_device_changed {
            let device_changed_callback: ffi::cubeb_device_changed_callback =
                Some(device_changed_cb_c::<F>);
            stream.register_device_changed_callback(device_changed_callback)?;
        }
        Ok(Stream::new(stream))
    }
}

// C callable callbacks
unsafe extern "C" fn data_cb_c<F>(
    _: *mut ffi::cubeb_stream,
    user_ptr: *mut c_void,
    input_buffer: *const c_void,
    output_buffer: *mut c_void,
    nframes: c_long,
) -> c_long {
    let ok = panic::catch_unwind(|| {
        let cbs = &mut *(user_ptr as *mut StreamCallbacks<F>);
        let input: &[F] = if input_buffer.is_null() {
            &[]
        } else {
            from_raw_parts(input_buffer as *const _, nframes as usize)
        };
        let output: &mut [F] = if output_buffer.is_null() {
            &mut []
        } else {
            from_raw_parts_mut(output_buffer as *mut _, nframes as usize)
        };
        (cbs.data)(input, output) as c_long
    });
    ok.unwrap_or(0)
}

unsafe extern "C" fn state_cb_c<F>(
    _: *mut ffi::cubeb_stream,
    user_ptr: *mut c_void,
    state: ffi::cubeb_state,
) {
    let ok = panic::catch_unwind(|| {
        let state = State::from(state);
        let cbs = &mut *(user_ptr as *mut StreamCallbacks<F>);
        (cbs.state)(state);
    });
    ok.expect("State callback panicked");
}

unsafe extern "C" fn device_changed_cb_c<F>(user_ptr: *mut c_void) {
    let ok = panic::catch_unwind(|| {
        let cbs = &mut *(user_ptr as *mut StreamCallbacks<F>);
        if let Some(ref mut device_changed) = cbs.device_changed {
            device_changed();
        }
    });
    ok.expect("Device changed callback panicked");
}
