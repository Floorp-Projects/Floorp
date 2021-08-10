// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

use crate::stream;
use crate::{assert_not_in_callback, run_in_callback};
use crate::{ClientStream, AUDIOIPC_INIT_PARAMS};
#[cfg(target_os = "linux")]
use audio_thread_priority::get_current_thread_info;
#[cfg(not(target_os = "linux"))]
use audio_thread_priority::promote_current_thread_to_real_time;
use audioipc::codec::LengthDelimitedCodec;
use audioipc::frame::{framed, Framed};
use audioipc::platformhandle_passing::{framed_with_platformhandles, FramedWithPlatformHandles};
use audioipc::{core, rpc};
use audioipc::{
    messages, messages::DeviceCollectionReq, messages::DeviceCollectionResp, ClientMessage,
    ServerMessage,
};
use cubeb_backend::{
    ffi, Context, ContextOps, DeviceCollectionRef, DeviceId, DeviceType, Error, Ops, Result,
    Stream, StreamParams, StreamParamsRef,
};
use futures::Future;
use futures_cpupool::{CpuFuture, CpuPool};
use std::ffi::{CStr, CString};
use std::os::raw::c_void;
use std::sync::mpsc;
use std::sync::{Arc, Mutex};
use std::thread;
use std::{fmt, mem, ptr};
use tokio::reactor;
use tokio::runtime::current_thread;

struct CubebClient;

impl rpc::Client for CubebClient {
    type Request = ServerMessage;
    type Response = ClientMessage;
    type Transport = FramedWithPlatformHandles<
        audioipc::AsyncMessageStream,
        LengthDelimitedCodec<Self::Request, Self::Response>,
    >;
}

pub const CLIENT_OPS: Ops = capi_new!(ClientContext, ClientStream);

// ClientContext's layout *must* match cubeb.c's `struct cubeb` for the
// common fields.
#[repr(C)]
pub struct ClientContext {
    _ops: *const Ops,
    rpc: rpc::ClientProxy<ServerMessage, ClientMessage>,
    core: core::CoreThread,
    cpu_pool: CpuPool,
    backend_id: CString,
    device_collection_rpc: bool,
    input_device_callback: Arc<Mutex<DeviceCollectionCallback>>,
    output_device_callback: Arc<Mutex<DeviceCollectionCallback>>,
}

impl ClientContext {
    #[doc(hidden)]
    pub fn handle(&self) -> current_thread::Handle {
        self.core.handle()
    }

    #[doc(hidden)]
    pub fn rpc(&self) -> rpc::ClientProxy<ServerMessage, ClientMessage> {
        self.rpc.clone()
    }

    #[doc(hidden)]
    pub fn cpu_pool(&self) -> CpuPool {
        self.cpu_pool.clone()
    }
}

#[cfg(target_os = "linux")]
fn promote_thread(rpc: &rpc::ClientProxy<ServerMessage, ClientMessage>) {
    match get_current_thread_info() {
        Ok(info) => {
            let bytes = info.serialize();
            // Don't wait for the response, this is on the callback thread, which must not block.
            rpc.call(ServerMessage::PromoteThreadToRealTime(bytes));
        }
        Err(_) => {
            warn!("Could not remotely promote thread to RT.");
        }
    }
}

#[cfg(not(target_os = "linux"))]
fn promote_thread(_rpc: &rpc::ClientProxy<ServerMessage, ClientMessage>) {
    match promote_current_thread_to_real_time(0, 48000) {
        Ok(_) => {
            info!("Audio thread promoted to real-time.");
        }
        Err(_) => {
            warn!("Could not promote thread to real-time.");
        }
    }
}

fn register_thread(callback: Option<extern "C" fn(*const ::std::os::raw::c_char)>) {
    if let Some(func) = callback {
        let thr = thread::current();
        let name = CString::new(thr.name().unwrap()).unwrap();
        func(name.as_ptr());
    }
}

fn unregister_thread(callback: Option<extern "C" fn()>) {
    if let Some(func) = callback {
        func();
    }
}

fn promote_and_register_thread(
    rpc: &rpc::ClientProxy<ServerMessage, ClientMessage>,
    callback: Option<extern "C" fn(*const ::std::os::raw::c_char)>,
) {
    promote_thread(rpc);
    register_thread(callback);
}

#[derive(Default)]
struct DeviceCollectionCallback {
    cb: ffi::cubeb_device_collection_changed_callback,
    user_ptr: usize,
}

struct DeviceCollectionServer {
    input_device_callback: Arc<Mutex<DeviceCollectionCallback>>,
    output_device_callback: Arc<Mutex<DeviceCollectionCallback>>,
    cpu_pool: CpuPool,
}

impl rpc::Server for DeviceCollectionServer {
    type Request = DeviceCollectionReq;
    type Response = DeviceCollectionResp;
    type Future = CpuFuture<Self::Response, ()>;
    type Transport =
        Framed<audioipc::AsyncMessageStream, LengthDelimitedCodec<Self::Response, Self::Request>>;

    fn process(&mut self, req: Self::Request) -> Self::Future {
        match req {
            DeviceCollectionReq::DeviceChange(device_type) => {
                trace!(
                    "ctx_thread: DeviceChange Callback: device_type={}",
                    device_type
                );

                let devtype = cubeb_backend::DeviceType::from_bits_truncate(device_type);

                let (input_cb, input_user_ptr) = {
                    let dcb = self.input_device_callback.lock().unwrap();
                    (dcb.cb, dcb.user_ptr)
                };
                let (output_cb, output_user_ptr) = {
                    let dcb = self.output_device_callback.lock().unwrap();
                    (dcb.cb, dcb.user_ptr)
                };

                self.cpu_pool.spawn_fn(move || {
                    run_in_callback(|| {
                        if devtype.contains(cubeb_backend::DeviceType::INPUT) {
                            unsafe {
                                input_cb.unwrap()(ptr::null_mut(), input_user_ptr as *mut c_void)
                            }
                        }
                        if devtype.contains(cubeb_backend::DeviceType::OUTPUT) {
                            unsafe {
                                output_cb.unwrap()(ptr::null_mut(), output_user_ptr as *mut c_void)
                            }
                        }
                    });

                    Ok(DeviceCollectionResp::DeviceChange)
                })
            }
        }
    }
}

impl ContextOps for ClientContext {
    fn init(_context_name: Option<&CStr>) -> Result<Context> {
        fn bind_and_send_client(
            stream: audioipc::AsyncMessageStream,
            tx_rpc: &mpsc::Sender<rpc::ClientProxy<ServerMessage, ClientMessage>>,
        ) {
            let transport = framed_with_platformhandles(stream, Default::default());
            let rpc = rpc::bind_client::<CubebClient>(transport);
            // If send fails then the rx end has closed
            // which is unlikely here.
            let _ = tx_rpc.send(rpc);
        }

        assert_not_in_callback();

        let (tx_rpc, rx_rpc) = mpsc::channel();

        let params = AUDIOIPC_INIT_PARAMS.with(|p| p.replace(None).unwrap());
        let thread_create_callback = params.thread_create_callback;
        let thread_destroy_callback = params.thread_destroy_callback;

        let server_stream =
            unsafe { audioipc::MessageStream::from_raw_fd(params.server_connection) };

        let core = core::spawn_thread(
            "AudioIPC Client RPC",
            move || {
                let handle = reactor::Handle::default();

                register_thread(thread_create_callback);

                let stream = server_stream.into_tokio_ipc(&handle).unwrap();
                bind_and_send_client(stream, &tx_rpc);
                Ok(())
            },
            move || unregister_thread(thread_destroy_callback),
        )
        .map_err(|_| Error::default())?;

        let rpc = rx_rpc.recv().map_err(|_| Error::default())?;
        let rpc2 = rpc.clone();

        // Don't let errors bubble from here.  Later calls against this context
        // will return errors the caller expects to handle.
        let _ = send_recv!(rpc, ClientConnect(std::process::id()) => ClientConnected);

        let backend_id = send_recv!(rpc, ContextGetBackendId => ContextBackendId())
            .unwrap_or_else(|_| "(remote error)".to_string());
        let backend_id = CString::new(backend_id).expect("backend_id query failed");

        let cpu_pool = futures_cpupool::Builder::new()
            .name_prefix("AudioIPC")
            .after_start(move || promote_and_register_thread(&rpc2, thread_create_callback))
            .before_stop(move || unregister_thread(thread_destroy_callback))
            .pool_size(params.pool_size)
            .stack_size(params.stack_size)
            .create();

        let ctx = Box::new(ClientContext {
            _ops: &CLIENT_OPS as *const _,
            rpc,
            core,
            cpu_pool,
            backend_id,
            device_collection_rpc: false,
            input_device_callback: Arc::new(Mutex::new(Default::default())),
            output_device_callback: Arc::new(Mutex::new(Default::default())),
        });
        Ok(unsafe { Context::from_ptr(Box::into_raw(ctx) as *mut _) })
    }

    fn backend_id(&mut self) -> &CStr {
        assert_not_in_callback();
        self.backend_id.as_c_str()
    }

    fn max_channel_count(&mut self) -> Result<u32> {
        assert_not_in_callback();
        send_recv!(self.rpc(), ContextGetMaxChannelCount => ContextMaxChannelCount())
    }

    fn min_latency(&mut self, params: StreamParams) -> Result<u32> {
        assert_not_in_callback();
        let params = messages::StreamParams::from(params.as_ref());
        send_recv!(self.rpc(), ContextGetMinLatency(params) => ContextMinLatency())
    }

    fn preferred_sample_rate(&mut self) -> Result<u32> {
        assert_not_in_callback();
        send_recv!(self.rpc(), ContextGetPreferredSampleRate => ContextPreferredSampleRate())
    }

    fn enumerate_devices(
        &mut self,
        devtype: DeviceType,
        collection: &DeviceCollectionRef,
    ) -> Result<()> {
        assert_not_in_callback();
        let v: Vec<ffi::cubeb_device_info> = match send_recv!(self.rpc(),
                             ContextGetDeviceEnumeration(devtype.bits()) =>
                             ContextEnumeratedDevices())
        {
            Ok(mut v) => v.drain(..).map(|i| i.into()).collect(),
            Err(e) => return Err(e),
        };
        let mut vs = v.into_boxed_slice();
        let coll = unsafe { &mut *collection.as_ptr() };
        coll.device = vs.as_mut_ptr();
        coll.count = vs.len();
        // Giving away the memory owned by vs.  Don't free it!
        // Reclaimed in `device_collection_destroy`.
        mem::forget(vs);
        Ok(())
    }

    fn device_collection_destroy(&mut self, collection: &mut DeviceCollectionRef) -> Result<()> {
        assert_not_in_callback();
        unsafe {
            let coll = &mut *collection.as_ptr();
            let mut devices = Vec::from_raw_parts(
                coll.device as *mut ffi::cubeb_device_info,
                coll.count,
                coll.count,
            );
            for dev in &mut devices {
                if !dev.device_id.is_null() {
                    let _ = CString::from_raw(dev.device_id as *mut _);
                }
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
            Ok(())
        }
    }

    fn stream_init(
        &mut self,
        stream_name: Option<&CStr>,
        input_device: DeviceId,
        input_stream_params: Option<&StreamParamsRef>,
        output_device: DeviceId,
        output_stream_params: Option<&StreamParamsRef>,
        latency_frames: u32,
        // These params aren't sent to the server
        data_callback: ffi::cubeb_data_callback,
        state_callback: ffi::cubeb_state_callback,
        user_ptr: *mut c_void,
    ) -> Result<Stream> {
        assert_not_in_callback();

        let stream_name = stream_name.map(|name| name.to_bytes_with_nul().to_vec());

        let input_stream_params = input_stream_params.map(messages::StreamParams::from);
        let output_stream_params = output_stream_params.map(messages::StreamParams::from);

        let init_params = messages::StreamInitParams {
            stream_name,
            input_device: input_device as usize,
            input_stream_params,
            output_device: output_device as usize,
            output_stream_params,
            latency_frames,
        };
        stream::init(self, init_params, data_callback, state_callback, user_ptr)
    }

    fn register_device_collection_changed(
        &mut self,
        devtype: DeviceType,
        collection_changed_callback: ffi::cubeb_device_collection_changed_callback,
        user_ptr: *mut c_void,
    ) -> Result<()> {
        assert_not_in_callback();

        if !self.device_collection_rpc {
            let fds = send_recv!(self.rpc(),
                                 ContextSetupDeviceCollectionCallback =>
                                 ContextSetupDeviceCollectionCallback())?;

            // TODO: The lowest comms layer expects exactly 3 PlatformHandles, but we only
            // need one here.  The server sent two dummy valid handles, ignore those (closed on drop)
            // and use the one we need.
            let stream =
                unsafe { audioipc::MessageStream::from_raw_fd(fds.platform_handles[0].into_raw()) };

            let server = DeviceCollectionServer {
                input_device_callback: self.input_device_callback.clone(),
                output_device_callback: self.output_device_callback.clone(),
                cpu_pool: self.cpu_pool(),
            };

            let (wait_tx, wait_rx) = mpsc::channel();
            self.handle()
                .spawn(futures::future::lazy(move || {
                    let handle = reactor::Handle::default();
                    let stream = stream.into_tokio_ipc(&handle).unwrap();
                    let transport = framed(stream, Default::default());
                    rpc::bind_server(transport, server);
                    wait_tx.send(()).unwrap();
                    Ok(())
                }))
                .expect("Failed to spawn DeviceCollectionServer");
            wait_rx.recv().unwrap();
            self.device_collection_rpc = true;
        }

        if devtype.contains(cubeb_backend::DeviceType::INPUT) {
            let mut cb = self.input_device_callback.lock().unwrap();
            cb.cb = collection_changed_callback;
            cb.user_ptr = user_ptr as usize;
        }
        if devtype.contains(cubeb_backend::DeviceType::OUTPUT) {
            let mut cb = self.output_device_callback.lock().unwrap();
            cb.cb = collection_changed_callback;
            cb.user_ptr = user_ptr as usize;
        }

        let enable = collection_changed_callback.is_some();
        send_recv!(self.rpc(),
                   ContextRegisterDeviceCollectionChanged(devtype.bits(), enable) =>
                   ContextRegisteredDeviceCollectionChanged)
    }
}

impl Drop for ClientContext {
    fn drop(&mut self) {
        debug!("ClientContext dropped...");
        let _ = send_recv!(self.rpc(), ClientDisconnect => ClientDisconnected);
    }
}

impl fmt::Debug for ClientContext {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("ClientContext")
            .field("_ops", &self._ops)
            .field("rpc", &self.rpc)
            .field("core", &self.core)
            .field("cpu_pool", &"...")
            .finish()
    }
}
