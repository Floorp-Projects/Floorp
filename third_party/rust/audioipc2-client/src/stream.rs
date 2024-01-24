// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

use crate::ClientContext;
use crate::{assert_not_in_callback, run_in_callback};
use audioipc::messages::StreamCreateParams;
use audioipc::messages::{self, CallbackReq, CallbackResp, ClientMessage, ServerMessage};
use audioipc::shm::SharedMem;
use audioipc::{rpccore, sys};
use cubeb_backend::{ffi, DeviceRef, Error, InputProcessingParams, Result, Stream, StreamOps};
use std::ffi::{CStr, CString};
use std::os::raw::c_void;
use std::ptr;
use std::sync::mpsc;
use std::sync::{Arc, Mutex};

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

// ClientStream's layout *must* match cubeb.c's `struct cubeb_stream` for the
// common fields.
#[repr(C)]
#[derive(Debug)]
pub struct ClientStream<'ctx> {
    // This must be a reference to Context for cubeb, cubeb accesses
    // stream methods via stream->context->ops
    context: &'ctx ClientContext,
    user_ptr: *mut c_void,
    token: usize,
    device_change_cb: Arc<Mutex<ffi::cubeb_device_changed_callback>>,
    // Signals ClientStream that CallbackServer has dropped.
    shutdown_rx: mpsc::Receiver<()>,
}

struct CallbackServer {
    shm: SharedMem,
    duplex_input: Option<Vec<u8>>,
    data_cb: ffi::cubeb_data_callback,
    state_cb: ffi::cubeb_state_callback,
    user_ptr: usize,
    device_change_cb: Arc<Mutex<ffi::cubeb_device_changed_callback>>,
    // Signals ClientStream that CallbackServer has dropped.
    _shutdown_tx: mpsc::Sender<()>,
}

impl rpccore::Server for CallbackServer {
    type ServerMessage = CallbackReq;
    type ClientMessage = CallbackResp;

    fn process(&mut self, req: Self::ServerMessage) -> Self::ClientMessage {
        match req {
            CallbackReq::Data {
                nframes,
                input_frame_size,
                output_frame_size,
            } => {
                trace!(
                    "stream_thread: Data Callback: nframes={} input_fs={} output_fs={}",
                    nframes,
                    input_frame_size,
                    output_frame_size,
                );

                let input_nbytes = nframes as usize * input_frame_size;
                let output_nbytes = nframes as usize * output_frame_size;

                // Input and output reuse the same shmem backing.  Unfortunately, cubeb's data_callback isn't
                // specified in such a way that would require the callee to consume all of the input before
                // writing to the output (i.e., it is passed as two pointers that aren't expected to alias).
                // That means we need to copy the input here.
                if let Some(buf) = &mut self.duplex_input {
                    assert!(input_nbytes > 0);
                    assert!(buf.capacity() >= input_nbytes);
                    unsafe {
                        let input = self.shm.get_slice(input_nbytes).unwrap();
                        ptr::copy_nonoverlapping(input.as_ptr(), buf.as_mut_ptr(), input.len());
                    }
                }

                run_in_callback(|| {
                    let nframes = unsafe {
                        let input_ptr = if input_frame_size > 0 {
                            if let Some(buf) = &mut self.duplex_input {
                                buf.as_ptr()
                            } else {
                                self.shm.get_slice(input_nbytes).unwrap().as_ptr()
                            }
                        } else {
                            ptr::null()
                        };
                        let output_ptr = if output_frame_size > 0 {
                            self.shm.get_mut_slice(output_nbytes).unwrap().as_mut_ptr()
                        } else {
                            ptr::null_mut()
                        };

                        self.data_cb.unwrap()(
                            ptr::null_mut(), // https://github.com/kinetiknz/cubeb/issues/518
                            self.user_ptr as *mut c_void,
                            input_ptr as *const _,
                            output_ptr as *mut _,
                            nframes as _,
                        )
                    };

                    CallbackResp::Data(nframes as isize)
                })
            }
            CallbackReq::State(state) => {
                trace!("stream_thread: State Callback: {:?}", state);
                run_in_callback(|| unsafe {
                    self.state_cb.unwrap()(ptr::null_mut(), self.user_ptr as *mut _, state);
                });

                CallbackResp::State
            }
            CallbackReq::DeviceChange => {
                run_in_callback(|| {
                    let cb = *self.device_change_cb.lock().unwrap();
                    if let Some(cb) = cb {
                        unsafe {
                            cb(self.user_ptr as *mut _);
                        }
                    } else {
                        warn!("DeviceChange received with null callback");
                    }
                });

                CallbackResp::DeviceChange
            }
        }
    }
}

impl<'ctx> ClientStream<'ctx> {
    fn init(
        ctx: &'ctx ClientContext,
        init_params: messages::StreamInitParams,
        data_callback: ffi::cubeb_data_callback,
        state_callback: ffi::cubeb_state_callback,
        user_ptr: *mut c_void,
    ) -> Result<Stream> {
        assert_not_in_callback();

        let rpc = ctx.rpc();
        let create_params = StreamCreateParams {
            input_stream_params: init_params.input_stream_params,
            output_stream_params: init_params.output_stream_params,
        };
        let mut data = send_recv!(rpc, StreamCreate(create_params) => StreamCreated())?;

        debug!(
            "token = {}, handle = {:?} area_size = {:?}",
            data.token, data.shm_handle, data.shm_area_size
        );

        let shm =
            match unsafe { SharedMem::from(data.shm_handle.take_handle(), data.shm_area_size) } {
                Ok(shm) => shm,
                Err(e) => {
                    warn!(
                        "SharedMem client mapping failed (size={}, err={:?})",
                        data.shm_area_size, e
                    );
                    return Err(Error::default());
                }
            };

        let duplex_input = if let (Some(_), Some(_)) = (
            init_params.input_stream_params,
            init_params.output_stream_params,
        ) {
            let mut duplex_input = Vec::new();
            match duplex_input.try_reserve_exact(data.shm_area_size) {
                Ok(()) => Some(duplex_input),
                Err(e) => {
                    warn!(
                        "duplex_input allocation failed (size={}, err={:?})",
                        data.shm_area_size, e
                    );
                    return Err(Error::default());
                }
            }
        } else {
            None
        };

        let mut stream =
            send_recv!(rpc, StreamInit(data.token, init_params) => StreamInitialized())?;
        let stream = unsafe { sys::Pipe::from_raw_handle(stream.take_handle()) };

        let user_data = user_ptr as usize;

        let null_cb: ffi::cubeb_device_changed_callback = None;
        let device_change_cb = Arc::new(Mutex::new(null_cb));

        let (_shutdown_tx, shutdown_rx) = mpsc::channel();

        let server = CallbackServer {
            shm,
            duplex_input,
            data_cb: data_callback,
            state_cb: state_callback,
            user_ptr: user_data,
            device_change_cb: device_change_cb.clone(),
            _shutdown_tx,
        };

        ctx.callback_handle()
            .bind_server(server, stream)
            .map_err(|_| Error::default())?;

        let stream = Box::into_raw(Box::new(ClientStream {
            context: ctx,
            user_ptr,
            token: data.token,
            device_change_cb,
            shutdown_rx,
        }));
        Ok(unsafe { Stream::from_ptr(stream as *mut _) })
    }
}

impl Drop for ClientStream<'_> {
    fn drop(&mut self) {
        debug!("ClientStream drop");
        let _ = send_recv!(self.context.rpc(), StreamDestroy(self.token) => StreamDestroyed);
        debug!("ClientStream drop - stream destroyed");
        // Wait for CallbackServer to shutdown.  The remote server drops the RPC
        // connection during StreamDestroy, which will cause CallbackServer to drop
        // once the connection close is detected.  Dropping CallbackServer will
        // cause the shutdown channel to error on recv, which we rely on to
        // synchronize with CallbackServer dropping.
        let _ = self.shutdown_rx.recv();
        debug!("ClientStream dropped");
    }
}

impl StreamOps for ClientStream<'_> {
    fn start(&mut self) -> Result<()> {
        assert_not_in_callback();
        let rpc = self.context.rpc();
        send_recv!(rpc, StreamStart(self.token) => StreamStarted)
    }

    fn stop(&mut self) -> Result<()> {
        assert_not_in_callback();
        let rpc = self.context.rpc();
        send_recv!(rpc, StreamStop(self.token) => StreamStopped)
    }

    fn position(&mut self) -> Result<u64> {
        assert_not_in_callback();
        let rpc = self.context.rpc();
        send_recv!(rpc, StreamGetPosition(self.token) => StreamPosition())
    }

    fn latency(&mut self) -> Result<u32> {
        assert_not_in_callback();
        let rpc = self.context.rpc();
        send_recv!(rpc, StreamGetLatency(self.token) => StreamLatency())
    }

    fn input_latency(&mut self) -> Result<u32> {
        assert_not_in_callback();
        let rpc = self.context.rpc();
        send_recv!(rpc, StreamGetInputLatency(self.token) => StreamInputLatency())
    }

    fn set_volume(&mut self, volume: f32) -> Result<()> {
        assert_not_in_callback();
        let rpc = self.context.rpc();
        send_recv!(rpc, StreamSetVolume(self.token, volume) => StreamVolumeSet)
    }

    fn set_name(&mut self, name: &CStr) -> Result<()> {
        assert_not_in_callback();
        let rpc = self.context.rpc();
        send_recv!(rpc, StreamSetName(self.token, name.to_owned()) => StreamNameSet)
    }

    fn current_device(&mut self) -> Result<&DeviceRef> {
        assert_not_in_callback();
        let rpc = self.context.rpc();
        match send_recv!(rpc, StreamGetCurrentDevice(self.token) => StreamCurrentDevice()) {
            Ok(d) => Ok(unsafe { DeviceRef::from_ptr(Box::into_raw(Box::new(d.into()))) }),
            Err(e) => Err(e),
        }
    }

    fn set_input_mute(&mut self, mute: bool) -> Result<()> {
        assert_not_in_callback();
        let rpc = self.context.rpc();
        send_recv!(rpc, StreamSetInputMute(self.token, mute) => StreamInputMuteSet)
    }

    fn set_input_processing_params(&mut self, params: InputProcessingParams) -> Result<()> {
        assert_not_in_callback();
        let rpc = self.context.rpc();
        send_recv!(rpc, StreamSetInputProcessingParams(self.token, params.bits()) => StreamInputProcessingParamsSet)
    }

    fn device_destroy(&mut self, device: &DeviceRef) -> Result<()> {
        assert_not_in_callback();
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
        device_changed_callback: ffi::cubeb_device_changed_callback,
    ) -> Result<()> {
        assert_not_in_callback();
        let rpc = self.context.rpc();
        let enable = device_changed_callback.is_some();
        *self.device_change_cb.lock().unwrap() = device_changed_callback;
        send_recv!(rpc, StreamRegisterDeviceChangeCallback(self.token, enable) => StreamRegisterDeviceChangeCallback)
    }
}

pub fn init(
    ctx: &ClientContext,
    init_params: messages::StreamInitParams,
    data_callback: ffi::cubeb_data_callback,
    state_callback: ffi::cubeb_state_callback,
    user_ptr: *mut c_void,
) -> Result<Stream> {
    let stm = ClientStream::init(ctx, init_params, data_callback, state_callback, user_ptr)?;
    debug_assert_eq!(stm.user_ptr(), user_ptr);
    Ok(stm)
}
