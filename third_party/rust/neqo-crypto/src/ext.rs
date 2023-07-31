// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::{
    agentio::as_c_void,
    constants::{Extension, HandshakeMessage, TLS_HS_CLIENT_HELLO, TLS_HS_ENCRYPTED_EXTENSIONS},
    err::Res,
    ssl::{
        PRBool, PRFileDesc, SECFailure, SECStatus, SECSuccess, SSLAlertDescription,
        SSLExtensionHandler, SSLExtensionWriter, SSLHandshakeType,
    },
};
use std::{
    cell::RefCell,
    convert::TryFrom,
    os::raw::{c_uint, c_void},
    pin::Pin,
    rc::Rc,
};

experimental_api!(SSL_InstallExtensionHooks(
    fd: *mut PRFileDesc,
    extension: u16,
    writer: SSLExtensionWriter,
    writer_arg: *mut c_void,
    handler: SSLExtensionHandler,
    handler_arg: *mut c_void,
));

pub enum ExtensionWriterResult {
    Write(usize),
    Skip,
}

pub enum ExtensionHandlerResult {
    Ok,
    Alert(crate::constants::Alert),
}

pub trait ExtensionHandler {
    fn write(&mut self, msg: HandshakeMessage, _d: &mut [u8]) -> ExtensionWriterResult {
        match msg {
            TLS_HS_CLIENT_HELLO | TLS_HS_ENCRYPTED_EXTENSIONS => ExtensionWriterResult::Write(0),
            _ => ExtensionWriterResult::Skip,
        }
    }

    fn handle(&mut self, msg: HandshakeMessage, _d: &[u8]) -> ExtensionHandlerResult {
        match msg {
            TLS_HS_CLIENT_HELLO | TLS_HS_ENCRYPTED_EXTENSIONS => ExtensionHandlerResult::Ok,
            _ => ExtensionHandlerResult::Alert(110), // unsupported_extension
        }
    }
}

type BoxedExtensionHandler = Box<Rc<RefCell<dyn ExtensionHandler>>>;

pub struct ExtensionTracker {
    extension: Extension,
    handler: Pin<Box<BoxedExtensionHandler>>,
}

impl ExtensionTracker {
    // Technically the as_mut() call here is the only unsafe bit,
    // but don't call this function lightly.
    unsafe fn wrap_handler_call<F, T>(arg: *mut c_void, f: F) -> T
    where
        F: FnOnce(&mut dyn ExtensionHandler) -> T,
    {
        let rc = arg.cast::<BoxedExtensionHandler>().as_mut().unwrap();
        f(&mut *rc.borrow_mut())
    }

    #[allow(clippy::cast_possible_truncation)]
    unsafe extern "C" fn extension_writer(
        _fd: *mut PRFileDesc,
        message: SSLHandshakeType::Type,
        data: *mut u8,
        len: *mut c_uint,
        max_len: c_uint,
        arg: *mut c_void,
    ) -> PRBool {
        let d = std::slice::from_raw_parts_mut(data, max_len as usize);
        Self::wrap_handler_call(arg, |handler| {
            // Cast is safe here because the message type is always part of the enum
            match handler.write(message as HandshakeMessage, d) {
                ExtensionWriterResult::Write(sz) => {
                    *len = c_uint::try_from(sz).expect("integer overflow from extension writer");
                    1
                }
                ExtensionWriterResult::Skip => 0,
            }
        })
    }

    unsafe extern "C" fn extension_handler(
        _fd: *mut PRFileDesc,
        message: SSLHandshakeType::Type,
        data: *const u8,
        len: c_uint,
        alert: *mut SSLAlertDescription,
        arg: *mut c_void,
    ) -> SECStatus {
        let d = std::slice::from_raw_parts(data, len as usize);
        #[allow(clippy::cast_possible_truncation)]
        Self::wrap_handler_call(arg, |handler| {
            // Cast is safe here because the message type is always part of the enum
            match handler.handle(message as HandshakeMessage, d) {
                ExtensionHandlerResult::Ok => SECSuccess,
                ExtensionHandlerResult::Alert(a) => {
                    *alert = a;
                    SECFailure
                }
            }
        })
    }

    /// Use the provided handler to manage an extension.  This is quite unsafe.
    ///
    /// # Safety
    /// The holder of this `ExtensionTracker` needs to ensure that it lives at
    /// least as long as the file descriptor, as NSS provides no way to remove
    /// an extension handler once it is configured.
    ///
    /// # Errors
    /// If the underlying NSS API fails to register a handler.
    pub unsafe fn new(
        fd: *mut PRFileDesc,
        extension: Extension,
        handler: Rc<RefCell<dyn ExtensionHandler>>,
    ) -> Res<Self> {
        // The ergonomics here aren't great for users of this API, but it's
        // horrific here. The pinned outer box gives us a stable pointer to the inner
        // box.  This is the pointer that is passed to NSS.
        //
        // The inner box points to the reference-counted object.  This inner box is
        // what we end up with a reference to in callbacks.  That extra wrapper around
        // the Rc avoid any touching of reference counts in callbacks, which would
        // inevitably lead to leaks as we don't control how many times the callback
        // is invoked.
        //
        // This way, only this "outer" code deals with the reference count.
        let mut tracker = Self {
            extension,
            handler: Box::pin(Box::new(handler)),
        };
        SSL_InstallExtensionHooks(
            fd,
            extension,
            Some(Self::extension_writer),
            as_c_void(&mut tracker.handler),
            Some(Self::extension_handler),
            as_c_void(&mut tracker.handler),
        )?;
        Ok(tracker)
    }
}

impl std::fmt::Debug for ExtensionTracker {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "ExtensionTracker: {:?}", self.extension)
    }
}
