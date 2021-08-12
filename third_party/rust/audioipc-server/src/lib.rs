// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details
#![warn(unused_extern_crates)]

#[macro_use]
extern crate error_chain;
#[macro_use]
extern crate log;

use audio_thread_priority::promote_current_thread_to_real_time;
use audioipc::core;
use audioipc::framing::framed;
use audioipc::rpc;
use audioipc::{MessageStream, PlatformHandle, PlatformHandleType};
use futures::sync::oneshot;
use futures::Future;
use once_cell::sync::Lazy;
use std::ffi::{CStr, CString};
use std::os::raw::c_void;
use std::ptr;
use std::sync::Mutex;
use tokio::reactor;

mod server;

struct CubebContextParams {
    context_name: CString,
    backend_name: Option<CString>,
}

static G_CUBEB_CONTEXT_PARAMS: Lazy<Mutex<CubebContextParams>> = Lazy::new(|| {
    Mutex::new(CubebContextParams {
        context_name: CString::new("AudioIPC Server").unwrap(),
        backend_name: None,
    })
});

#[allow(deprecated)]
pub mod errors {
    #![allow(clippy::upper_case_acronyms)]
    error_chain! {
        links {
            AudioIPC(::audioipc::errors::Error, ::audioipc::errors::ErrorKind);
        }
        foreign_links {
            Cubeb(cubeb_core::Error);
            Io(::std::io::Error);
            Canceled(::futures::sync::oneshot::Canceled);
        }
    }
}

use crate::errors::*;

struct ServerWrapper {
    core_thread: core::CoreThread,
    callback_thread: core::CoreThread,
}

fn run() -> Result<ServerWrapper> {
    trace!("Starting up cubeb audio server event loop thread...");

    let callback_thread = core::spawn_thread(
        "AudioIPC Callback RPC",
        || {
            match promote_current_thread_to_real_time(0, 48000) {
                Ok(_) => {}
                Err(_) => {
                    debug!("Failed to promote audio callback thread to real-time.");
                }
            }
            trace!("Starting up cubeb audio callback event loop thread...");
            Ok(())
        },
        || {},
    )
    .map_err(|e| {
        debug!(
            "Failed to start cubeb audio callback event loop thread: {:?}",
            e
        );
        e
    })?;

    let core_thread = core::spawn_thread(
        "AudioIPC Server RPC",
        move || {
            audioipc::server_platform_init();
            Ok(())
        },
        || {},
    )
    .map_err(|e| {
        debug!("Failed to cubeb audio core event loop thread: {:?}", e);
        e
    })?;

    Ok(ServerWrapper {
        core_thread,
        callback_thread,
    })
}

#[allow(clippy::missing_safety_doc)]
#[no_mangle]
pub unsafe extern "C" fn audioipc_server_start(
    context_name: *const std::os::raw::c_char,
    backend_name: *const std::os::raw::c_char,
) -> *mut c_void {
    let mut params = G_CUBEB_CONTEXT_PARAMS.lock().unwrap();
    if !context_name.is_null() {
        params.context_name = CStr::from_ptr(context_name).to_owned();
    }
    if !backend_name.is_null() {
        let backend_string = CStr::from_ptr(backend_name).to_owned();
        params.backend_name = Some(backend_string);
    }
    match run() {
        Ok(server) => Box::into_raw(Box::new(server)) as *mut _,
        Err(_) => ptr::null_mut() as *mut _,
    }
}

#[no_mangle]
pub extern "C" fn audioipc_server_new_client(
    p: *mut c_void,
    shm_area_size: usize,
) -> PlatformHandleType {
    let (wait_tx, wait_rx) = oneshot::channel();
    let wrapper: &ServerWrapper = unsafe { &*(p as *mut _) };

    let callback_thread_handle = wrapper.callback_thread.handle();

    // We create a connected pair of anonymous IPC endpoints. One side
    // is registered with the reactor core, the other side is returned
    // to the caller.
    MessageStream::anonymous_ipc_pair()
        .map(|(ipc_server, ipc_client)| {
            // Spawn closure to run on same thread as reactor::Core
            // via remote handle.
            wrapper
                .core_thread
                .handle()
                .spawn(futures::future::lazy(move || {
                    trace!("Incoming connection");
                    let handle = reactor::Handle::default();
                    ipc_server.into_tokio_ipc(&handle)
                    .map(|sock| {
                        let transport = framed(sock, Default::default());
                        rpc::bind_server(transport, server::CubebServer::new(callback_thread_handle, shm_area_size));
                    }).map_err(|_| ())
                    // Notify waiting thread that server has been registered.
                    .and_then(|_| wait_tx.send(()))
                }))
                .expect("Failed to spawn CubebServer");
            // Wait for notification that server has been registered
            // with reactor::Core.
            let _ = wait_rx.wait();
            unsafe { PlatformHandle::from(ipc_client).into_raw() }
        })
        .unwrap_or(audioipc::INVALID_HANDLE_VALUE)
}

#[no_mangle]
pub extern "C" fn audioipc_server_stop(p: *mut c_void) {
    let wrapper = unsafe { Box::<ServerWrapper>::from_raw(p as *mut _) };
    drop(wrapper);
}
