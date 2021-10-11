/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use crate::gecko_bindings::{bindings, structs::mozilla};
use crate::json_writer::JSONWriter;
use crate::marker::deserializer_tags_state::{
    get_marker_type_functions_read_guard, MarkerTypeFunctions,
};
use std::ops::DerefMut;
use std::os::raw::{c_char, c_void};

#[no_mangle]
pub unsafe extern "C" fn gecko_profiler_serialize_marker_for_tag(
    deserializer_tag: u8,
    payload: *const u8,
    payload_size: usize,
    json_writer: &mut mozilla::baseprofiler::SpliceableJSONWriter,
) {
    let marker_type_functions = get_marker_type_functions_read_guard();
    let &MarkerTypeFunctions {
        transmute_and_stream_fn,
        marker_type_name_fn,
        ..
    } = marker_type_functions.get(deserializer_tag);
    let mut json_writer = JSONWriter::new(&mut *json_writer);

    // Serialize the marker type name first.
    json_writer.string_property("type", marker_type_name_fn());
    // Serialize the marker payload now.
    transmute_and_stream_fn(payload, payload_size, &mut json_writer);
}

#[no_mangle]
pub unsafe extern "C" fn gecko_profiler_stream_marker_schemas(
    json_writer: &mut mozilla::baseprofiler::SpliceableJSONWriter,
    streamed_names_set: *mut c_void,
) {
    let marker_type_functions = get_marker_type_functions_read_guard();

    for funcs in marker_type_functions.iter() {
        let marker_name = (funcs.marker_type_name_fn)();
        let mut marker_schema = (funcs.marker_type_display_fn)();

        bindings::gecko_profiler_marker_schema_stream(
            json_writer,
            marker_name.as_ptr() as *const c_char,
            marker_name.len(),
            marker_schema.pin.deref_mut().as_mut_ptr(),
            streamed_names_set,
        )
    }
}
