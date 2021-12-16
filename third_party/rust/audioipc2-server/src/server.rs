// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

#[cfg(target_os = "linux")]
use audio_thread_priority::{promote_thread_to_real_time, RtPriorityThreadInfo};
use audioipc::messages::SerializableHandle;
use audioipc::messages::{
    CallbackReq, CallbackResp, ClientMessage, Device, DeviceCollectionReq, DeviceCollectionResp,
    DeviceInfo, RegisterDeviceCollectionChanged, ServerMessage, StreamCreate, StreamCreateParams,
    StreamInitParams, StreamParams,
};
use audioipc::shm::SharedMem;
use audioipc::{ipccore, rpccore, sys};
use cubeb_core as cubeb;
use cubeb_core::ffi;
use std::convert::From;
use std::ffi::CStr;
use std::mem::size_of;
use std::os::raw::{c_long, c_void};
use std::rc::Rc;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::{cell::RefCell, sync::Mutex};
use std::{panic, slice};

use crate::errors::*;

fn error(error: cubeb::Error) -> ClientMessage {
    ClientMessage::Error(error.raw_code())
}

struct CubebDeviceCollectionManager {
    servers: Mutex<Vec<Rc<RefCell<DeviceCollectionChangeCallback>>>>,
}

impl CubebDeviceCollectionManager {
    fn new() -> CubebDeviceCollectionManager {
        CubebDeviceCollectionManager {
            servers: Mutex::new(Vec::new()),
        }
    }

    fn register(
        &mut self,
        context: &cubeb::Context,
        server: &Rc<RefCell<DeviceCollectionChangeCallback>>,
        devtype: cubeb::DeviceType,
    ) -> cubeb::Result<()> {
        let mut servers = self.servers.lock().unwrap();
        if servers.is_empty() {
            self.internal_register(context, true)?;
        }
        server.borrow_mut().devtype.insert(devtype);
        if !servers.iter().any(|s| Rc::ptr_eq(s, server)) {
            servers.push(server.clone());
        }
        Ok(())
    }

    fn unregister(
        &mut self,
        context: &cubeb::Context,
        server: &Rc<RefCell<DeviceCollectionChangeCallback>>,
        devtype: cubeb::DeviceType,
    ) -> cubeb::Result<()> {
        let mut servers = self.servers.lock().unwrap();
        server.borrow_mut().devtype.remove(devtype);
        if server.borrow().devtype.is_empty() {
            servers.retain(|s| !Rc::ptr_eq(s, server));
        }
        if servers.is_empty() {
            self.internal_register(context, false)?;
        }
        Ok(())
    }

    fn internal_register(&self, context: &cubeb::Context, enable: bool) -> cubeb::Result<()> {
        let user_ptr = if enable {
            self as *const CubebDeviceCollectionManager as *mut c_void
        } else {
            std::ptr::null_mut()
        };
        for &(dir, cb) in &[
            (
                cubeb::DeviceType::INPUT,
                device_collection_changed_input_cb_c as _,
            ),
            (
                cubeb::DeviceType::OUTPUT,
                device_collection_changed_output_cb_c as _,
            ),
        ] {
            unsafe {
                context.register_device_collection_changed(
                    dir,
                    if enable { Some(cb) } else { None },
                    user_ptr,
                )?;
            }
        }
        Ok(())
    }

    // Warning: this is called from an internal cubeb thread, so we must not mutate unprotected shared state.
    unsafe fn device_collection_changed_callback(&self, device_type: ffi::cubeb_device_type) {
        let servers = self.servers.lock().unwrap();
        servers.iter().for_each(|server| {
            if server
                .borrow()
                .devtype
                .contains(cubeb::DeviceType::from_bits_truncate(device_type))
            {
                server
                    .borrow_mut()
                    .device_collection_changed_callback(device_type)
            }
        });
    }
}

struct DevIdMap {
    devices: Vec<usize>,
}

// A cubeb_devid is an opaque type which may be implemented with a stable
// pointer in a cubeb backend.  cubeb_devids received remotely must be
// validated before use, so DevIdMap provides a simple 1:1 mapping between a
// cubeb_devid and an IPC-transportable value suitable for use as a unique
// handle.
impl DevIdMap {
    fn new() -> DevIdMap {
        let mut d = DevIdMap {
            devices: Vec::with_capacity(32),
        };
        // A null cubeb_devid is used for selecting the default device.
        // Pre-populate the mapping with 0 -> 0 to handle nulls.
        d.devices.push(0);
        d
    }

    // Given a cubeb_devid, return a unique stable value suitable for use
    // over IPC.
    fn make_handle(&mut self, devid: usize) -> usize {
        if let Some(i) = self.devices.iter().position(|&d| d == devid) {
            return i;
        }
        self.devices.push(devid);
        self.devices.len() - 1
    }

    // Given a handle produced by `make_handle`, return the associated
    // cubeb_devid.  Invalid handles result in a panic.
    fn handle_to_id(&self, handle: usize) -> usize {
        self.devices[handle]
    }
}

struct CubebContextState {
    context: cubeb::Result<cubeb::Context>,
    manager: CubebDeviceCollectionManager,
}

thread_local!(static CONTEXT_KEY: RefCell<Option<CubebContextState>> = RefCell::new(None));

fn cubeb_init_from_context_params() -> cubeb::Result<cubeb::Context> {
    let params = super::G_CUBEB_CONTEXT_PARAMS.lock().unwrap();
    let context_name = Some(params.context_name.as_c_str());
    let backend_name = params.backend_name.as_deref();
    let r = cubeb::Context::init(context_name, backend_name);
    r.map_err(|e| {
        info!("cubeb::Context::init failed r={:?}", e);
        e
    })
}

fn with_local_context<T, F>(f: F) -> T
where
    F: FnOnce(&cubeb::Result<cubeb::Context>, &mut CubebDeviceCollectionManager) -> T,
{
    CONTEXT_KEY.with(|k| {
        let mut state = k.borrow_mut();
        if state.is_none() {
            *state = Some(CubebContextState {
                context: cubeb_init_from_context_params(),
                manager: CubebDeviceCollectionManager::new(),
            });
        }
        let CubebContextState { context, manager } = state.as_mut().unwrap();
        // Always reattempt to initialize cubeb, OS config may have changed.
        if context.is_err() {
            *context = cubeb_init_from_context_params();
        }
        f(context, manager)
    })
}

struct DeviceCollectionClient;

impl rpccore::Client for DeviceCollectionClient {
    type ServerMessage = DeviceCollectionReq;
    type ClientMessage = DeviceCollectionResp;
}

struct CallbackClient;

impl rpccore::Client for CallbackClient {
    type ServerMessage = CallbackReq;
    type ClientMessage = CallbackResp;
}

struct ServerStreamCallbacks {
    /// Size of input frame in bytes
    input_frame_size: u16,
    /// Size of output frame in bytes
    output_frame_size: u16,
    /// Shared memory buffer for transporting audio data to/from client
    shm: SharedMem,
    /// RPC interface to callback server running in client
    rpc: rpccore::Proxy<CallbackReq, CallbackResp>,
}

impl ServerStreamCallbacks {
    fn data_callback(&mut self, input: &[u8], output: &mut [u8], nframes: isize) -> isize {
        trace!(
            "Stream data callback: {} {} {}",
            nframes,
            input.len(),
            output.len()
        );

        unsafe {
            if self.input_frame_size != 0 {
                self.shm
                    .get_mut_slice(input.len())
                    .unwrap()
                    .copy_from_slice(input);
            }
        }

        let r = self
            .rpc
            .call(CallbackReq::Data {
                nframes,
                input_frame_size: self.input_frame_size as usize,
                output_frame_size: self.output_frame_size as usize,
            })
            .wait();

        match r {
            Ok(CallbackResp::Data(frames)) => {
                if frames >= 0 {
                    let nbytes = frames as usize * self.output_frame_size as usize;
                    trace!("Reslice output to {}", nbytes);
                    unsafe {
                        if self.output_frame_size != 0 {
                            output[..nbytes].copy_from_slice(self.shm.get_slice(nbytes).unwrap());
                        }
                    }
                }
                frames
            }
            _ => {
                debug!("Unexpected message {:?} during data_callback", r);
                // TODO: Return a CUBEB_ERROR result here once
                // https://github.com/kinetiknz/cubeb/issues/553 is
                // fixed.
                0
            }
        }
    }

    fn state_callback(&mut self, state: cubeb::State) {
        trace!("Stream state callback: {:?}", state);
        let r = self.rpc.call(CallbackReq::State(state.into())).wait();
        match r {
            Ok(CallbackResp::State) => {}
            _ => {
                debug!("Unexpected message {:?} during state callback", r);
            }
        }
    }

    fn device_change_callback(&mut self) {
        trace!("Stream device change callback");
        let r = self.rpc.call(CallbackReq::DeviceChange).wait();
        match r {
            Ok(CallbackResp::DeviceChange) => {}
            _ => {
                debug!("Unexpected message {:?} during device change callback", r);
            }
        }
    }
}

static SHM_ID: AtomicUsize = AtomicUsize::new(0);

// Generate a temporary shm_id fragment that is unique to the process.  This
// path is used temporarily to create a shm segment, which is then
// immediately deleted from the filesystem while retaining handles to the
// shm to be shared between the server and client.
fn get_shm_id() -> String {
    format!(
        "cubeb-shm-{}-{}",
        std::process::id(),
        SHM_ID.fetch_add(1, Ordering::SeqCst)
    )
}

struct ServerStream {
    stream: Option<cubeb::Stream>,
    cbs: Box<ServerStreamCallbacks>,
    shm_setup: Option<rpccore::ProxyResponse<CallbackResp>>,
}

impl Drop for ServerStream {
    fn drop(&mut self) {
        // `stream` *must* be dropped before `cbs`.
        drop(self.stream.take());
    }
}

struct DeviceCollectionChangeCallback {
    rpc: rpccore::Proxy<DeviceCollectionReq, DeviceCollectionResp>,
    devtype: cubeb::DeviceType,
}

impl DeviceCollectionChangeCallback {
    fn device_collection_changed_callback(&mut self, device_type: ffi::cubeb_device_type) {
        // TODO: Assert device_type is in devtype.
        debug!(
            "Sending device collection ({:?}) changed event",
            device_type
        );
        let _ = self
            .rpc
            .call(DeviceCollectionReq::DeviceChange(device_type))
            .wait();
    }
}

pub struct CubebServer {
    callback_thread: ipccore::EventLoopHandle,
    device_collection_thread: ipccore::EventLoopHandle,
    streams: slab::Slab<ServerStream>,
    remote_pid: Option<u32>,
    device_collection_change_callbacks: Option<Rc<RefCell<DeviceCollectionChangeCallback>>>,
    devidmap: DevIdMap,
    shm_area_size: usize,
}

#[allow(unknown_lints)] // non_send_fields_in_send_ty is Nightly-only as of 2021-11-29.
#[allow(clippy::non_send_fields_in_send_ty)]
// XXX: required for server setup, verify this is safe.
unsafe impl Send for CubebServer {}

impl rpccore::Server for CubebServer {
    type ServerMessage = ServerMessage;
    type ClientMessage = ClientMessage;

    fn process(&mut self, req: Self::ServerMessage) -> Self::ClientMessage {
        if let ServerMessage::ClientConnect(pid) = req {
            self.remote_pid = Some(pid);
        }
        with_local_context(|context, manager| match *context {
            Err(_) => error(cubeb::Error::error()),
            Ok(ref context) => self.process_msg(context, manager, &req),
        })
    }
}

// Debugging for BMO 1594216/1612044.
macro_rules! try_stream {
    ($self:expr, $stm_tok:expr) => {
        if $self.streams.contains($stm_tok) {
            $self.streams[$stm_tok]
                .stream
                .as_mut()
                .expect("uninitialized stream")
        } else {
            error!(
                "{}:{}:{} - Stream({}): invalid token",
                file!(),
                line!(),
                column!(),
                $stm_tok
            );
            return error(cubeb::Error::invalid_parameter());
        }
    };
}

impl CubebServer {
    pub fn new(
        callback_thread: ipccore::EventLoopHandle,
        device_collection_thread: ipccore::EventLoopHandle,
        shm_area_size: usize,
    ) -> Self {
        CubebServer {
            callback_thread,
            device_collection_thread,
            streams: slab::Slab::<ServerStream>::new(),
            remote_pid: None,
            device_collection_change_callbacks: None,
            devidmap: DevIdMap::new(),
            shm_area_size,
        }
    }

    // Process a request coming from the client.
    fn process_msg(
        &mut self,
        context: &cubeb::Context,
        manager: &mut CubebDeviceCollectionManager,
        msg: &ServerMessage,
    ) -> ClientMessage {
        let resp: ClientMessage = match *msg {
            ServerMessage::ClientConnect(_) => {
                // remote_pid is set before cubeb initialization, just verify here.
                assert!(self.remote_pid.is_some());
                ClientMessage::ClientConnected
            }

            ServerMessage::ClientDisconnect => {
                // TODO:
                //self.connection.client_disconnect();
                ClientMessage::ClientDisconnected
            }

            ServerMessage::ContextGetBackendId => {
                ClientMessage::ContextBackendId(context.backend_id().to_string())
            }

            ServerMessage::ContextGetMaxChannelCount => context
                .max_channel_count()
                .map(ClientMessage::ContextMaxChannelCount)
                .unwrap_or_else(error),

            ServerMessage::ContextGetMinLatency(ref params) => {
                let format = cubeb::SampleFormat::from(params.format);
                let layout = cubeb::ChannelLayout::from(params.layout);

                let params = cubeb::StreamParamsBuilder::new()
                    .format(format)
                    .rate(params.rate)
                    .channels(params.channels)
                    .layout(layout)
                    .take();

                context
                    .min_latency(&params)
                    .map(ClientMessage::ContextMinLatency)
                    .unwrap_or_else(error)
            }

            ServerMessage::ContextGetPreferredSampleRate => context
                .preferred_sample_rate()
                .map(ClientMessage::ContextPreferredSampleRate)
                .unwrap_or_else(error),

            ServerMessage::ContextGetDeviceEnumeration(device_type) => context
                .enumerate_devices(cubeb::DeviceType::from_bits_truncate(device_type))
                .map(|devices| {
                    let v: Vec<DeviceInfo> = devices
                        .iter()
                        .map(|i| {
                            let mut tmp: DeviceInfo = i.as_ref().into();
                            // Replace each cubeb_devid with a unique handle suitable for IPC.
                            tmp.devid = self.devidmap.make_handle(tmp.devid);
                            tmp
                        })
                        .collect();
                    ClientMessage::ContextEnumeratedDevices(v)
                })
                .unwrap_or_else(error),

            ServerMessage::StreamCreate(ref params) => self
                .process_stream_create(params)
                .unwrap_or_else(|_| error(cubeb::Error::error())),

            ServerMessage::StreamInit(stm_tok, ref params) => self
                .process_stream_init(context, stm_tok, params)
                .unwrap_or_else(|_| error(cubeb::Error::error())),

            ServerMessage::StreamDestroy(stm_tok) => {
                if self.streams.contains(stm_tok) {
                    debug!("Unregistering stream {:?}", stm_tok);
                    self.streams.remove(stm_tok);
                } else {
                    // Debugging for BMO 1594216/1612044.
                    error!("StreamDestroy({}): invalid token", stm_tok);
                    return error(cubeb::Error::invalid_parameter());
                }
                ClientMessage::StreamDestroyed
            }

            ServerMessage::StreamStart(stm_tok) => try_stream!(self, stm_tok)
                .start()
                .map(|_| ClientMessage::StreamStarted)
                .unwrap_or_else(error),

            ServerMessage::StreamStop(stm_tok) => try_stream!(self, stm_tok)
                .stop()
                .map(|_| ClientMessage::StreamStopped)
                .unwrap_or_else(error),

            ServerMessage::StreamGetPosition(stm_tok) => try_stream!(self, stm_tok)
                .position()
                .map(ClientMessage::StreamPosition)
                .unwrap_or_else(error),

            ServerMessage::StreamGetLatency(stm_tok) => try_stream!(self, stm_tok)
                .latency()
                .map(ClientMessage::StreamLatency)
                .unwrap_or_else(error),

            ServerMessage::StreamGetInputLatency(stm_tok) => try_stream!(self, stm_tok)
                .input_latency()
                .map(ClientMessage::StreamInputLatency)
                .unwrap_or_else(error),

            ServerMessage::StreamSetVolume(stm_tok, volume) => try_stream!(self, stm_tok)
                .set_volume(volume)
                .map(|_| ClientMessage::StreamVolumeSet)
                .unwrap_or_else(error),

            ServerMessage::StreamSetName(stm_tok, ref name) => try_stream!(self, stm_tok)
                .set_name(name)
                .map(|_| ClientMessage::StreamNameSet)
                .unwrap_or_else(error),

            ServerMessage::StreamGetCurrentDevice(stm_tok) => try_stream!(self, stm_tok)
                .current_device()
                .map(|device| ClientMessage::StreamCurrentDevice(Device::from(device)))
                .unwrap_or_else(error),

            ServerMessage::StreamRegisterDeviceChangeCallback(stm_tok, enable) => {
                try_stream!(self, stm_tok)
                    .register_device_changed_callback(if enable {
                        Some(device_change_cb_c)
                    } else {
                        None
                    })
                    .map(|_| ClientMessage::StreamRegisterDeviceChangeCallback)
                    .unwrap_or_else(error)
            }

            ServerMessage::ContextSetupDeviceCollectionCallback => {
                let (server_pipe, client_pipe) = match sys::make_pipe_pair() {
                    Ok((server_pipe, client_pipe)) => (server_pipe, client_pipe),
                    Err(e) => {
                        debug!(
                            "ContextSetupDeviceCollectionCallback - make_pipe_pair failed: {:?}",
                            e
                        );
                        return error(cubeb::Error::error());
                    }
                };

                // TODO: this should bind the client_pipe and send the server_pipe to the remote, but
                //       additional work is required as it's not possible to convert a Windows sys::Pipe into a raw handle.
                // TODO: Use the rpc_thread instead of an extra device_collection_thread, but a reentrant bind_client
                //       is required to support that.
                let rpc = match self
                    .device_collection_thread
                    .bind_client::<DeviceCollectionClient>(server_pipe)
                {
                    Ok(rpc) => rpc,
                    Err(e) => {
                        debug!(
                            "ContextSetupDeviceCollectionCallback - bind_client: {:?}",
                            e
                        );
                        return error(cubeb::Error::error());
                    }
                };

                self.device_collection_change_callbacks =
                    Some(Rc::new(RefCell::new(DeviceCollectionChangeCallback {
                        rpc,
                        devtype: cubeb::DeviceType::empty(),
                    })));
                let fd = RegisterDeviceCollectionChanged {
                    platform_handle: SerializableHandle::new(client_pipe, self.remote_pid.unwrap()),
                };

                ClientMessage::ContextSetupDeviceCollectionCallback(fd)
            }

            ServerMessage::ContextRegisterDeviceCollectionChanged(device_type, enable) => self
                .process_register_device_collection_changed(
                    context,
                    manager,
                    cubeb::DeviceType::from_bits_truncate(device_type),
                    enable,
                )
                .unwrap_or_else(error),

            #[cfg(target_os = "linux")]
            ServerMessage::PromoteThreadToRealTime(thread_info) => {
                let info = RtPriorityThreadInfo::deserialize(thread_info);
                match promote_thread_to_real_time(info, 0, 48000) {
                    Ok(_) => {
                        info!("Promotion of content process thread to real-time OK");
                    }
                    Err(_) => {
                        warn!("Promotion of content process thread to real-time error");
                    }
                }
                ClientMessage::ThreadPromoted
            }
        };

        trace!("process_msg: req={:?}, resp={:?}", msg, resp);

        resp
    }

    fn process_register_device_collection_changed(
        &mut self,
        context: &cubeb::Context,
        manager: &mut CubebDeviceCollectionManager,
        devtype: cubeb::DeviceType,
        enable: bool,
    ) -> cubeb::Result<ClientMessage> {
        if devtype == cubeb::DeviceType::UNKNOWN {
            return Err(cubeb::Error::invalid_parameter());
        }

        assert!(self.device_collection_change_callbacks.is_some());
        let cbs = self.device_collection_change_callbacks.as_ref().unwrap();

        if enable {
            manager.register(context, cbs, devtype)
        } else {
            manager.unregister(context, cbs, devtype)
        }
        .map(|_| ClientMessage::ContextRegisteredDeviceCollectionChanged)
    }

    // Stream create is special, so it's been separated from process_msg.
    fn process_stream_create(&mut self, params: &StreamCreateParams) -> Result<ClientMessage> {
        fn frame_size_in_bytes(params: Option<&StreamParams>) -> u16 {
            params
                .map(|p| {
                    let format = p.format.into();
                    let sample_size = match format {
                        cubeb::SampleFormat::S16LE
                        | cubeb::SampleFormat::S16BE
                        | cubeb::SampleFormat::S16NE => 2,
                        cubeb::SampleFormat::Float32LE
                        | cubeb::SampleFormat::Float32BE
                        | cubeb::SampleFormat::Float32NE => 4,
                    };
                    let channel_count = p.channels as u16;
                    sample_size * channel_count
                })
                .unwrap_or(0u16)
        }

        // Create the callback handling struct which is attached the cubeb stream.
        let input_frame_size = frame_size_in_bytes(params.input_stream_params.as_ref());
        let output_frame_size = frame_size_in_bytes(params.output_stream_params.as_ref());

        // Estimate a safe shmem size for this stream configuration.  If the server was configured with a fixed
        // shm_area_size override, use that instead.
        // TODO: Add a new cubeb API to query the precise buffer size required for a given stream config.
        // https://github.com/mozilla/audioipc-2/issues/124
        let shm_area_size = if self.shm_area_size == 0 {
            let frame_size = output_frame_size.max(input_frame_size) as u32;
            let in_rate = params.input_stream_params.map(|p| p.rate).unwrap_or(0);
            let out_rate = params.output_stream_params.map(|p| p.rate).unwrap_or(0);
            let rate = out_rate.max(in_rate);
            // 1s of audio, rounded up to the nearest 64kB.
            (((rate * frame_size) + 0xffff) & !0xffff) as usize
        } else {
            self.shm_area_size
        };
        debug!("shm_area_size = {}", shm_area_size);

        let shm = SharedMem::new(&get_shm_id(), shm_area_size)?;
        let shm_handle = unsafe { shm.make_handle()? };

        let (server_pipe, client_pipe) = sys::make_pipe_pair()?;
        // TODO: this should bind the client_pipe and send the server_pipe to the remote, but
        //       additional work is required as it's not possible to convert a Windows sys::Pipe into a raw handle.
        let rpc = self
            .callback_thread
            .bind_client::<CallbackClient>(server_pipe)?;

        // Send shm configuration to client but don't wait for result; that'll be checked in process_stream_init.
        let shm_setup = Some(rpc.call(CallbackReq::SharedMem(
            SerializableHandle::new(shm_handle, self.remote_pid.unwrap()),
            shm_area_size,
        )));

        let cbs = Box::new(ServerStreamCallbacks {
            input_frame_size,
            output_frame_size,
            shm,
            rpc,
        });

        let entry = self.streams.vacant_entry();
        let key = entry.key();
        debug!("Registering stream {:?}", key);

        entry.insert(ServerStream {
            stream: None,
            shm_setup,
            cbs,
        });

        Ok(ClientMessage::StreamCreated(StreamCreate {
            token: key,
            platform_handle: SerializableHandle::new(client_pipe, self.remote_pid.unwrap()),
        }))
    }

    // Stream init is special, so it's been separated from process_msg.
    fn process_stream_init(
        &mut self,
        context: &cubeb::Context,
        stm_tok: usize,
        params: &StreamInitParams,
    ) -> Result<ClientMessage> {
        // Create cubeb stream from params
        let stream_name = params
            .stream_name
            .as_ref()
            .and_then(|name| CStr::from_bytes_with_nul(name).ok());

        // Map IPC handle back to cubeb_devid.
        let input_device = self.devidmap.handle_to_id(params.input_device) as *const _;
        let input_stream_params = params.input_stream_params.as_ref().map(|isp| unsafe {
            cubeb::StreamParamsRef::from_ptr(isp as *const StreamParams as *mut _)
        });

        // Map IPC handle back to cubeb_devid.
        let output_device = self.devidmap.handle_to_id(params.output_device) as *const _;
        let output_stream_params = params.output_stream_params.as_ref().map(|osp| unsafe {
            cubeb::StreamParamsRef::from_ptr(osp as *const StreamParams as *mut _)
        });

        let latency = params.latency_frames;

        let server_stream = &mut self.streams[stm_tok];
        assert!(size_of::<Box<ServerStreamCallbacks>>() == size_of::<usize>());
        let user_ptr = server_stream.cbs.as_ref() as *const ServerStreamCallbacks as *mut c_void;

        // SharedMem setup message should've been processed by client by now.
        let shm_setup = server_stream
            .shm_setup
            .take()
            .expect("invalid shm_setup state");
        match shm_setup.wait() {
            Ok(CallbackResp::SharedMem) => {}
            Ok(CallbackResp::Error(e)) => {
                // If the client replied with an error (e.g. client OOM), log error and fail stream init.
                debug!(
                    "Shmem setup for stream {:?} failed (raw error {:?})",
                    stm_tok, e
                );
                return Ok(ClientMessage::Error(e));
            }
            Ok(r) => {
                debug!(
                    "Shmem setup for stream {:?} failed (unexpected response {:?})",
                    stm_tok, r
                );
                return Ok(error(cubeb::Error::error()));
            }
            Err(e) => {
                // If the client errored before responding, log error and fail stream init.
                debug!(
                    "Shmem setup for stream {:?} failed (error {:?})",
                    stm_tok, e
                );
                return Err(e.into());
            }
        }

        let stream = unsafe {
            let stream = context.stream_init(
                stream_name,
                input_device,
                input_stream_params,
                output_device,
                output_stream_params,
                latency,
                Some(data_cb_c),
                Some(state_cb_c),
                user_ptr,
            );
            match stream {
                Ok(stream) => stream,
                Err(e) => {
                    debug!("Unregistering stream {:?} (stream error {:?})", stm_tok, e);
                    self.streams.remove(stm_tok);
                    return Err(e.into());
                }
            }
        };

        server_stream.stream = Some(stream);

        Ok(ClientMessage::StreamInitialized)
    }
}

// C callable callbacks
unsafe extern "C" fn data_cb_c(
    _: *mut ffi::cubeb_stream,
    user_ptr: *mut c_void,
    input_buffer: *const c_void,
    output_buffer: *mut c_void,
    nframes: c_long,
) -> c_long {
    let ok = panic::catch_unwind(|| {
        let cbs = &mut *(user_ptr as *mut ServerStreamCallbacks);
        let input = if input_buffer.is_null() {
            &[]
        } else {
            let nbytes = nframes * c_long::from(cbs.input_frame_size);
            slice::from_raw_parts(input_buffer as *const u8, nbytes as usize)
        };
        let output: &mut [u8] = if output_buffer.is_null() {
            &mut []
        } else {
            let nbytes = nframes * c_long::from(cbs.output_frame_size);
            slice::from_raw_parts_mut(output_buffer as *mut u8, nbytes as usize)
        };
        cbs.data_callback(input, output, nframes as isize) as c_long
    });
    // TODO: Return a CUBEB_ERROR result here once
    // https://github.com/kinetiknz/cubeb/issues/553 is fixed.
    ok.unwrap_or(0)
}

unsafe extern "C" fn state_cb_c(
    _: *mut ffi::cubeb_stream,
    user_ptr: *mut c_void,
    state: ffi::cubeb_state,
) {
    let ok = panic::catch_unwind(|| {
        let state = cubeb::State::from(state);
        let cbs = &mut *(user_ptr as *mut ServerStreamCallbacks);
        cbs.state_callback(state);
    });
    ok.expect("State callback panicked");
}

unsafe extern "C" fn device_change_cb_c(user_ptr: *mut c_void) {
    let ok = panic::catch_unwind(|| {
        let cbs = &mut *(user_ptr as *mut ServerStreamCallbacks);
        cbs.device_change_callback();
    });
    ok.expect("Device change callback panicked");
}

unsafe extern "C" fn device_collection_changed_input_cb_c(
    _: *mut ffi::cubeb,
    user_ptr: *mut c_void,
) {
    let ok = panic::catch_unwind(|| {
        let manager = &mut *(user_ptr as *mut CubebDeviceCollectionManager);
        manager.device_collection_changed_callback(ffi::CUBEB_DEVICE_TYPE_INPUT);
    });
    ok.expect("Collection changed (input) callback panicked");
}

unsafe extern "C" fn device_collection_changed_output_cb_c(
    _: *mut ffi::cubeb,
    user_ptr: *mut c_void,
) {
    let ok = panic::catch_unwind(|| {
        let manager = &mut *(user_ptr as *mut CubebDeviceCollectionManager);
        manager.device_collection_changed_callback(ffi::CUBEB_DEVICE_TYPE_OUTPUT);
    });
    ok.expect("Collection changed (output) callback panicked");
}
