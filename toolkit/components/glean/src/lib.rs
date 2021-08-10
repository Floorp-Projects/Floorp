// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! Firefox on Glean (FOG) is the name of the layer that integrates the [Glean SDK][glean-sdk] into Firefox Desktop.
//! It is currently being designed and implemented.
//!
//! The [Glean SDK][glean-sdk] is a data collection library built by Mozilla for use in its products.
//! Like [Telemetry][telemetry], it can be used to
//! (in accordance with our [Privacy Policy][privacy-policy])
//! send anonymous usage statistics to Mozilla in order to make better decisions.
//!
//! Documentation can be found online in the [Firefox Source Docs][docs].
//!
//! [glean-sdk]: https://github.com/mozilla/glean/
//! [book-of-glean]: https://mozilla.github.io/glean/book/index.html
//! [privacy-policy]: https://www.mozilla.org/privacy/
//! [docs]: https://firefox-source-docs.mozilla.org/toolkit/components/glean/

// No one is currently using the Glean SDK, so let's export it, so we know it gets
// compiled.
pub extern crate fog;

use nserror::{nsresult, NS_ERROR_FAILURE, NS_OK};
use nsstring::nsACString;

#[cfg(not(target_os = "android"))]
#[macro_use]
extern crate cstr;
#[cfg(not(target_os = "android"))]
#[macro_use]
extern crate xpcom;

#[cfg(not(target_os = "android"))]
mod init;

#[cfg(target_os = "android")]
#[path = "android_init.rs"]
mod init;

pub use init::fog_init;

#[no_mangle]
pub unsafe extern "C" fn fog_shutdown() {
    glean::shutdown();
}

static mut PENDING_BUF: Vec<u8> = Vec::new();

// IPC serialization/deserialization methods
// Crucially important that the first two not be called on multiple threads.

/// Only safe if only called on a single thread (the same single thread you call
/// fog_give_ipc_buf on).
#[no_mangle]
pub unsafe extern "C" fn fog_serialize_ipc_buf() -> usize {
    if let Some(buf) = fog::ipc::take_buf() {
        PENDING_BUF = buf;
        PENDING_BUF.len()
    } else {
        PENDING_BUF = vec![];
        0
    }
}

/// Only safe if called on a single thread (the same single thread you call
/// fog_serialize_ipc_buf on), and if buf points to an allocated buffer of at
/// least buf_len bytes.
#[no_mangle]
pub unsafe extern "C" fn fog_give_ipc_buf(buf: *mut u8, buf_len: usize) -> usize {
    let pending_len = PENDING_BUF.len();
    if buf.is_null() || buf_len < pending_len {
        return 0;
    }
    std::ptr::copy_nonoverlapping(PENDING_BUF.as_ptr(), buf, pending_len);
    PENDING_BUF = Vec::new();
    pending_len
}

/// Only safe if buf points to an allocated buffer of at least buf_len bytes.
/// No ownership is transfered to Rust by this method: caller owns the memory at
/// buf before and after this call.
#[no_mangle]
pub unsafe extern "C" fn fog_use_ipc_buf(buf: *const u8, buf_len: usize) {
    let slice = std::slice::from_raw_parts(buf, buf_len);
    let res = fog::ipc::replay_from_buf(slice);
    if res.is_err() {
        log::warn!("Unable to replay ipc buffer. This will result in data loss.");
        fog::metrics::fog_ipc::replay_failures.add(1);
    }
}

/// Sets the debug tag for pings assembled in the future.
/// Returns an error result if the provided value is not a valid tag.
#[no_mangle]
pub unsafe extern "C" fn fog_set_debug_view_tag(value: &nsACString) -> nsresult {
    let result = glean::set_debug_view_tag(&value.to_string());
    if result {
        return NS_OK;
    } else {
        return NS_ERROR_FAILURE;
    }
}

/// Submits a ping by name.
#[no_mangle]
pub unsafe extern "C" fn fog_submit_ping(ping_name: &nsACString) -> nsresult {
    glean::submit_ping_by_name(&ping_name.to_string(), None);
    NS_OK
}

/// Turns ping logging on or off.
/// Returns an error if the logging failed to be configured.
#[no_mangle]
pub unsafe extern "C" fn fog_set_log_pings(value: bool) -> nsresult {
    glean::set_log_pings(value);
    NS_OK
}
