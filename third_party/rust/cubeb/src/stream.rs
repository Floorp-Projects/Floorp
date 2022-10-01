// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use cubeb_core;
use ffi;
use std::ffi::CString;
use std::marker::PhantomData;
use std::mem::ManuallyDrop;
use std::os::raw::{c_long, c_void};
use std::slice::{from_raw_parts, from_raw_parts_mut};
use std::{ops, panic, ptr};
use {ContextRef, DeviceId, Error, Result, State, StreamParamsRef};

/// User supplied data callback.
///
/// - Calling other cubeb functions from this callback is unsafe.
/// - The code in the callback should be non-blocking.
/// - Returning less than the number of frames this callback asks for or
///   provides puts the stream in drain mode. This callback will not be called
///   again, and the state callback will be called with CUBEB_STATE_DRAINED when
///   all the frames have been output.
///
/// # Arguments
///
/// - `input_buffer`: A slice containing the input data, zero-len if this is an output-only stream.
/// - `output_buffer`: A mutable slice to be filled with audio samples, zero-len if this is an input-only stream.
///
/// # Return value
///
/// If the stream has output, this is the number of frames written to the output buffer. In this
/// case, if this number is less than the length of the output buffer, then the stream will start to
/// drain.
///
/// If the stream is input only, then returning the length of the input buffer indicates data has
/// been read.  In this case, a value less than that will result in the stream being stopped.
pub type DataCallback<F> = dyn FnMut(&[F], &mut [F]) -> isize + Send + Sync + 'static;

/// User supplied state callback.
///
/// # Arguments
///
/// `state`: The new state of the stream
pub type StateCallback = dyn FnMut(State) + Send + Sync + 'static;

/// User supplied callback called when the underlying device changed.
pub type DeviceChangedCallback = dyn FnMut() + Send + Sync + 'static;

pub struct StreamCallbacks<F> {
    pub(crate) data: Box<DataCallback<F>>,
    pub(crate) state: Box<StateCallback>,
    pub(crate) device_changed: Option<Box<DeviceChangedCallback>>,
}

/// Audio input/output stream
///
/// # Example
/// ```no_run
/// extern crate cubeb;
/// use std::thread;
/// use std::time::Duration;
///
/// type Frame = cubeb::MonoFrame<f32>;
///
/// fn main() {
///     let ctx = cubeb::init("Cubeb tone example").unwrap();
///
///     let params = cubeb::StreamParamsBuilder::new()
///         .format(cubeb::SampleFormat::Float32LE)
///         .rate(44_100)
///         .channels(1)
///         .layout(cubeb::ChannelLayout::MONO)
///         .prefs(cubeb::StreamPrefs::NONE)
///         .take();
///
///     let phase_inc = 440.0 / 44_100.0;
///     let mut phase = 0.0;
///     let volume = 0.25;
///
///     let mut builder = cubeb::StreamBuilder::<Frame>::new();
///     builder
///         .name("Cubeb Square Wave")
///         .default_output(&params)
///         .latency(0x1000)
///         .data_callback(move |_, output| {
///             // Generate a square wave
///             for x in output.iter_mut() {
///                 x.m = if phase < 0.5 { volume } else { -volume };
///                 phase = (phase + phase_inc) % 1.0;
///             }
///
///             output.len() as isize
///         })
///         .state_callback(|state| {
///             println!("stream {:?}", state);
///         });
///     let stream = builder.init(&ctx).expect("Failed to create stream.");
///
///     // Start playback
///     stream.start().unwrap();
///
///     // Play for 1/2 second
///     thread::sleep(Duration::from_millis(500));
///
///     // Shutdown
///     stream.stop().unwrap();
/// }
/// ```
pub struct Stream<F>(ManuallyDrop<cubeb_core::Stream>, PhantomData<*const F>);

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

/// Stream builder
///
/// ```no_run
/// use cubeb::{Context, MonoFrame, Sample};
/// use std::f32::consts::PI;
/// use std::thread;
/// use std::time::Duration;
///
/// const SAMPLE_FREQUENCY: u32 = 48_000;
/// const STREAM_FORMAT: cubeb::SampleFormat = cubeb::SampleFormat::S16LE;
/// type Frame = MonoFrame<i16>;
///
/// let ctx = Context::init(None, None).unwrap();
///
/// let params = cubeb::StreamParamsBuilder::new()
///     .format(STREAM_FORMAT)
///     .rate(SAMPLE_FREQUENCY)
///     .channels(1)
///     .layout(cubeb::ChannelLayout::MONO)
///     .take();
///
/// let mut position = 0u32;
///
/// let mut builder = cubeb::StreamBuilder::<Frame>::new();
/// builder
///     .name("Cubeb tone (mono)")
///     .default_output(&params)
///     .latency(0x1000)
///     .data_callback(move |_, output| {
///         // generate our test tone on the fly
///         for f in output.iter_mut() {
///             // North American dial tone
///             let t1 = (2.0 * PI * 350.0 * position as f32 / SAMPLE_FREQUENCY as f32).sin();
///             let t2 = (2.0 * PI * 440.0 * position as f32 / SAMPLE_FREQUENCY as f32).sin();
///
///             f.m = i16::from_float(0.5 * (t1 + t2));
///
///             position += 1;
///         }
///         output.len() as isize
///     })
///     .state_callback(|state| {
///         println!("stream {:?}", state);
///     });
///
/// let stream = builder.init(&ctx).expect("Failed to create cubeb stream");
/// ```
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
        Default::default()
    }

    /// User supplied data callback, see [`DataCallback`]
    pub fn data_callback<D>(&mut self, cb: D) -> &mut Self
    where
        D: FnMut(&[F], &mut [F]) -> isize + Send + Sync + 'static,
    {
        self.data_cb = Some(Box::new(cb) as Box<DataCallback<F>>);
        self
    }

    /// User supplied state callback, see [`StateCallback`]
    pub fn state_callback<S>(&mut self, cb: S) -> &mut Self
    where
        S: FnMut(State) + Send + Sync + 'static,
    {
        self.state_cb = Some(Box::new(cb) as Box<StateCallback>);
        self
    }

    /// A name for this stream.
    pub fn name<T: Into<Vec<u8>>>(&mut self, name: T) -> &mut Self {
        self.name = Some(CString::new(name).unwrap());
        self
    }

    /// Use the default input device with `params`
    ///
    /// Optional if the stream is output only
    pub fn default_input(&mut self, params: &'a StreamParamsRef) -> &mut Self {
        self.input = Some((ptr::null(), params));
        self
    }

    /// Use a specific input device with `params`
    ///
    /// Optional if the stream is output only
    pub fn input(&mut self, device: DeviceId, params: &'a StreamParamsRef) -> &mut Self {
        self.input = Some((device, params));
        self
    }

    /// Use the default output device with `params`
    ///
    /// Optional if the stream is input only
    pub fn default_output(&mut self, params: &'a StreamParamsRef) -> &mut Self {
        self.output = Some((ptr::null(), params));
        self
    }

    /// Use a specific output device with `params`
    ///
    /// Optional if the stream is input only
    pub fn output(&mut self, device: DeviceId, params: &'a StreamParamsRef) -> &mut Self {
        self.output = Some((device, params));
        self
    }

    /// Stream latency in frames.
    ///
    /// Valid range is [1, 96000].
    pub fn latency(&mut self, latency: u32) -> &mut Self {
        self.latency = Some(latency);
        self
    }

    /// User supplied callback called when the underlying device changed.
    ///
    /// See [`StateCallback`]
    ///
    /// Optional
    pub fn device_changed_cb<CB>(&mut self, cb: CB) -> &mut Self
    where
        CB: FnMut() + Send + Sync + 'static,
    {
        self.device_changed_cb = Some(Box::new(cb) as Box<DeviceChangedCallback>);
        self
    }

    /// Build the stream
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

        let stream_name = self.name.as_deref();
        let (input_device, input_stream_params) =
            self.input.map_or((ptr::null(), None), |x| (x.0, Some(x.1)));
        let (output_device, output_stream_params) = self
            .output
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
                cbs as *mut _,
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

impl<'a, F> Default for StreamBuilder<'a, F> {
    fn default() -> Self {
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
