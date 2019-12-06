// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use ffi_support::{ConcurrentHandleMap, FfiStr};
use lazy_static::lazy_static;

use glean_core::metrics::PingType;

use crate::ffi_string_ext::FallibleToString;
use crate::handlemap_ext::HandleMapExtension;
use crate::GLEAN;

lazy_static! {
    pub(crate) static ref PING_TYPES: ConcurrentHandleMap<PingType> = ConcurrentHandleMap::new();
}
crate::define_infallible_handle_map_deleter!(PING_TYPES, glean_destroy_ping_type);

#[no_mangle]
pub extern "C" fn glean_new_ping_type(
    ping_name: FfiStr,
    include_client_id: u8,
    send_if_empty: u8,
) -> u64 {
    PING_TYPES.insert_with_log(|| {
        let ping_name = ping_name.to_string_fallible()?;
        Ok(PingType::new(
            ping_name,
            include_client_id != 0,
            send_if_empty != 0,
        ))
    })
}

#[no_mangle]
pub extern "C" fn glean_test_has_ping_type(glean_handle: u64, ping_name: FfiStr) -> u8 {
    GLEAN.call_infallible(glean_handle, |glean| {
        glean.get_ping_by_name(ping_name.as_str()).is_some()
    })
}

#[no_mangle]
pub extern "C" fn glean_register_ping_type(glean_handle: u64, ping_type_handle: u64) {
    PING_TYPES.call_infallible(ping_type_handle, |ping_type| {
        GLEAN.call_infallible_mut(glean_handle, |glean| glean.register_ping_type(ping_type))
    })
}
