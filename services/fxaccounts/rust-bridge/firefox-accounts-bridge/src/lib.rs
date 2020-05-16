/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This crate, as its name suggests, serves as a bridge
//! between the Javascript world (RustFxAccount.js)
//! and the Rust world (fxa-client crate).
//!
//! The `bridge` module implements the `mozIFirefoxAccountsBridge`
//! interface, which is callable from JS.
//!
//! The `punt` module helps running the synchronous Rust operations
//! on a background thread pool managed by Gecko.

#[macro_use]
extern crate cstr;
#[macro_use]
extern crate xpcom;

mod bridge;
mod punt;

use crate::bridge::Bridge;
use nserror::{nsresult, NS_OK};
use xpcom::{interfaces::mozIFirefoxAccountsBridge, RefPtr};

// The constructor for our fxa implementation, exposed to C++. See
// `FirefoxAccountsBridge.h` for the declaration and the wrapper we
// register with the component manager.
#[no_mangle]
pub unsafe extern "C" fn NS_NewFirefoxAccountsBridge(
    result: *mut *const mozIFirefoxAccountsBridge,
) -> nsresult {
    match Bridge::new() {
        Ok(bridge) => {
            RefPtr::new(bridge.coerce::<mozIFirefoxAccountsBridge>()).forget(&mut *result);
            NS_OK
        }
        Err(err) => err.into(),
    }
}
