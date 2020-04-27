/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![allow(non_snake_case)]

//! This crate bridges the WebExtension storage area interfaces in Firefox
//! Desktop to the extension storage Rust component in Application Services.
//!
//! ## How are the WebExtension storage APIs implemented in Firefox?
//!
//! There are three storage APIs available for WebExtensions:
//! `storage.local`, which is stored locally in an IndexedDB database and never
//! synced to other devices, `storage.sync`, which is stored in a local SQLite
//! database and synced to all devices signed in to the same Firefox Account,
//! and `storage.managed`, which is provisioned in a native manifest and
//! read-only.
//!
//! * `storage.local` is implemented in `ExtensionStorageIDB.jsm`.
//! * `storage.sync` is implemented in a Rust component, `webext_storage`. This
//!   Rust component is vendored in m-c, and exposed to JavaScript via an XPCOM
//!   API in `webext_storage_bridge` (this crate). Eventually, we'll change
//!   `ExtensionStorageSync.jsm` to call the XPCOM API instead of using the
//!   old Kinto storage adapter.
//! * `storage.managed` is implemented directly in `parent/ext-storage.js`.
//!
//! `webext_storage_bridge` implements the `mozIExtensionStorageArea`
//! (and, eventually, `mozIBridgedSyncEngine`) interface for `storage.sync`. The
//! implementation is in `area::StorageSyncArea`, and is backed by the
//! `webext_storage` component.

#[macro_use]
extern crate cstr;
#[macro_use]
extern crate xpcom;

mod area;
mod error;
mod op;
mod store;

use nserror::{nsresult, NS_OK};
use xpcom::{interfaces::mozIConfigurableExtensionStorageArea, RefPtr};

use crate::area::StorageSyncArea;

/// The constructor for a `storage.sync` area. This uses C linkage so that it
/// can be called from C++. See `ExtensionStorageComponents.h` for the C++
/// constructor that's passed to the component manager.
///
/// # Safety
///
/// This function is unsafe because it dereferences `result`.
#[no_mangle]
pub unsafe extern "C" fn NS_NewExtensionStorageSyncArea(
    result: *mut *const mozIConfigurableExtensionStorageArea,
) -> nsresult {
    match StorageSyncArea::new() {
        Ok(bridge) => {
            RefPtr::new(bridge.coerce::<mozIConfigurableExtensionStorageArea>())
                .forget(&mut *result);
            NS_OK
        }
        Err(err) => err.into(),
    }
}
