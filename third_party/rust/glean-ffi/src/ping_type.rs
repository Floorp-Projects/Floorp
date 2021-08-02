// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use ffi_support::{ConcurrentHandleMap, FfiStr};
use once_cell::sync::Lazy;

use glean_core::metrics::PingType;

use crate::ffi_string_ext::FallibleToString;
use crate::handlemap_ext::HandleMapExtension;
use crate::{from_raw_string_array, with_glean_value, with_glean_value_mut, RawStringArray};

pub(crate) static PING_TYPES: Lazy<ConcurrentHandleMap<PingType>> =
    Lazy::new(ConcurrentHandleMap::new);
crate::define_infallible_handle_map_deleter!(PING_TYPES, glean_destroy_ping_type);

#[no_mangle]
pub extern "C" fn glean_new_ping_type(
    ping_name: FfiStr,
    include_client_id: u8,
    send_if_empty: u8,
    reason_codes: RawStringArray,
    reason_codes_len: i32,
) -> u64 {
    PING_TYPES.insert_with_log(|| {
        let ping_name = ping_name.to_string_fallible()?;
        let reason_codes = from_raw_string_array(reason_codes, reason_codes_len)?;
        Ok(PingType::new(
            ping_name,
            include_client_id != 0,
            send_if_empty != 0,
            reason_codes,
        ))
    })
}

#[no_mangle]
pub extern "C" fn glean_test_has_ping_type(ping_name: FfiStr) -> u8 {
    with_glean_value(|glean| glean.get_ping_by_name(ping_name.as_str()).is_some() as u8)
}

#[no_mangle]
pub extern "C" fn glean_register_ping_type(ping_type_handle: u64) {
    PING_TYPES.call_infallible(ping_type_handle, |ping_type| {
        with_glean_value_mut(|glean| glean.register_ping_type(ping_type))
    })
}
