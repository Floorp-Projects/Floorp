// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

use crate::ClientContext;
use crate::{assert_not_in_callback, run_in_callback};
use audioipc::frame::{framed, Framed};
use audioipc::messages::{self, CallbackReq, CallbackResp, ClientMessage, ServerMessage};
use audioipc::rpc;
use audioipc::shm::SharedMem;
use audioipc::{codec::LengthDelimitedCodec, messages::StreamCreateParams};
use cubeb_backend::{ffi, DeviceRef, Error, Result, Stream, StreamOps};
use futures::Future;
use futures_cpupool::{CpuFuture, CpuPool};
use std::ffi::{CStr, CString};
use std::os::raw::c_void;
use std::ptr;
use std::sync::mpsc;
use std::sync::{Arc, Mutex};
use tokio::reactor;

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
    input_shm: Option<SharedMem>,
    output_shm: Option<SharedMem>,
    data_cb: ffi::cubeb_data_callback,
    state_cb: ffi::cubeb_state_callback,
    user_ptr: usize,
    cpu_pool: CpuPool,
    device_change_cb: Arc<Mutex<ffi::cubeb_device_changed_callback>>,
    // Signals ClientStream that CallbackServer has dropped.
    _shutdown_tx: mpsc::Sender<()>,
}

impl rpc::Server for CallbackServer {
    type Request = CallbackReq;
    type Response = CallbackResp;
    type Future = CpuFuture<Self::Response, ()>;
    type Transport =
        Framed<audioipc::AsyncMessageStream, LengthDelimitedCodec<Self::Response, Self::Request>>;

    fn process(&mut self, req: Self::Request) -> Self::Future {
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

                // Clone values that need to be moved into the cpu pool thread.
                let input_shm = unsafe { self.input_shm.as_ref().map(|shm| shm.unsafe_view()) };
                let output_shm = unsafe { self.output_shm.as_ref().map(|shm| shm.unsafe_view()) };
                let user_ptr = self.user_ptr;
                let cb = self.data_cb.unwrap();

                self.cpu_pool.spawn_fn(move || {
                    let input_ptr = match input_shm {
                        Some(shm) => unsafe {
                            shm.get_slice(nframes as usize * input_frame_size)
                                .unwrap()
                                .as_ptr()
                        },
                        None => ptr::null(),
                    };
                    let output_ptr = match output_shm {
                        Some(mut shm) => unsafe {
                            shm.get_mut_slice(nframes as usize * output_frame_size)
                                .unwrap()
                                .as_mut_ptr()
                        },
                        None => ptr::null(),
                    };

                    run_in_callback(|| {
                        let nframes = unsafe {
                            cb(
                                ptr::null_mut(),
                                user_ptr as *mut c_void,
                                input_ptr as *const _,
                                output_ptr as *mut _,
                                nframes as _,
                            )
                        };

                        Ok(CallbackResp::Data(nframes as isize))
                    })
                })
            }
            CallbackReq::State(state) => {
                trace!("stream_thread: State Callback: {:?}", state);
                let user_ptr = self.user_ptr;
                let cb = self.state_cb.unwrap();
                self.cpu_pool.spawn_fn(move || {
                    run_in_callback(|| unsafe {
                        cb(ptr::null_mut(), user_ptr as *mut _, state);
                    });

                    Ok(CallbackResp::State)
                })
            }
            CallbackReq::DeviceChange => {
                let cb = self.device_change_cb.clone();
                let user_ptr = self.user_ptr;
                self.cpu_pool.spawn_fn(move || {
                    run_in_callback(|| {
                        let cb = cb.lock().unwrap();
                        if let Some(cb) = *cb {
                            unsafe {
                                cb(user_ptr as *mut _);
                            }
                        } else {
                            warn!("DeviceChange received with null callback");
                        }
                    });

                    Ok(CallbackResp::DeviceChange)
                })
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
        let data = send_recv!(rpc, StreamCreate(create_params) => StreamCreated())?;

        debug!(
            "token = {}, handles = {:?}",
            data.token, data.platform_handles
        );

        let has_input = init_params.input_stream_params.is_some();
        let has_output = init_params.output_stream_params.is_some();

        let stream =
            unsafe { audioipc::MessageStream::from_raw_fd(data.platform_handles[0].into_raw()) };

        let input_shm = if has_input {
            match unsafe { SharedMem::from(&data.platform_handles[1], audioipc::SHM_AREA_SIZE) } {
                Ok(shm) => Some(shm),
                Err(e) => {
                    debug!("Client failed to set up input shmem: {}", e);
                    return Err(Error::error());
                }
            }
        } else {
            None
        };

        let output_shm = if has_output {
            match unsafe { SharedMem::from(&data.platform_handles[2], audioipc::SHM_AREA_SIZE) } {
                Ok(shm) => Some(shm),
                Err(e) => {
                    debug!("Client failed to set up output shmem: {}", e);
                    return Err(Error::error());
                }
            }
        } else {
            None
        };

        let user_data = user_ptr as usize;

        let cpu_pool = ctx.cpu_pool();

        let null_cb: ffi::cubeb_device_changed_callback = None;
        let device_change_cb = Arc::new(Mutex::new(null_cb));

        let (_shutdown_tx, shutdown_rx) = mpsc::channel();

        let server = CallbackServer {
            input_shm,
            output_shm,
            data_cb: data_callback,
            state_cb: state_callback,
            user_ptr: user_data,
            cpu_pool,
            device_change_cb: device_change_cb.clone(),
            _shutdown_tx,
        };

        let (wait_tx, wait_rx) = mpsc::channel();
        ctx.handle()
            .spawn(futures::future::lazy(move || {
                let handle = reactor::Handle::default();
                let stream = stream.into_tokio_ipc(&handle).unwrap();
                let transport = framed(stream, Default::default());
                rpc::bind_server(transport, server);
                wait_tx.send(()).unwrap();
                Ok(())
            }))
            .expect("Failed to spawn CallbackServer");
        wait_rx.recv().unwrap();

        send_recv!(rpc, StreamInit(data.token, init_params) => StreamInitialized)?;

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

impl<'ctx> Drop for ClientStream<'ctx> {
    fn drop(&mut self) {
        debug!("ClientStream drop");
        let rpc = self.context.rpc();
        let _ = send_recv!(rpc, StreamDestroy(self.token) => StreamDestroyed);
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

impl<'ctx> StreamOps for ClientStream<'ctx> {
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

    fn device_destroy(&mut self, device: &DeviceRef) -> Result<()> {
        assert_not_in_callback();
        // It's all unsafe...
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
