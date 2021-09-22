/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::json_writer::JSONWriter;
use crate::marker::schema::MarkerSchema;
use crate::marker::{transmute_and_stream, ProfilerMarker};
use std::collections::HashMap;
use std::sync::{RwLock, RwLockReadGuard};

lazy_static! {
    static ref DESERIALIZER_TAGS_STATE: RwLock<DeserializerTagsState> =
        RwLock::new(DeserializerTagsState::new());
}

/// A state that keeps track of each marker types and their deserializer tags.
/// They are added during the marker insertion and read during the marker serialization.
pub struct DeserializerTagsState {
    /// C++ side accepts only u8 values, but we only know usize values as the
    /// unique marker type values. So, we need to keep track of each
    /// "marker tag -> deserializer tag" conversions to directly get the
    /// deserializer tags of the already added marker types.
    pub marker_tag_to_deserializer_tag: HashMap<usize, u8>,
    /// Vector of marker type functions.
    /// 1-based, i.e.: [0] -> tag 1. Elements are pushed to the end of the vector
    /// whenever a new marker type is used in a Firefox session; the content is
    /// kept between profiler runs in that session. On the C++ side, we have the
    /// same algorithm (althought it's a sized array). See `sMarkerTypeFunctions1Based`.
    pub marker_type_functions_1_based: Vec<MarkerTypeFunctions>,
}

/// Functions that will be stored per marker type, so we can serialize the marker
/// schema and stream the marker payload for a specific type.
pub struct MarkerTypeFunctions {
    /// A function that returns the name of the marker type.
    pub marker_type_name_fn: fn() -> &'static str,
    /// A function that returns a `MarkerSchema`, which contains all the
    /// information needed to stream the display schema associated with a
    /// marker type.
    pub marker_type_display_fn: fn() -> MarkerSchema,
    /// A function that can read a serialized payload from bytes and streams it
    /// as JSON object properties.
    pub transmute_and_stream_fn:
        unsafe fn(payload: *const u8, payload_size: usize, json_writer: &mut JSONWriter),
}

impl DeserializerTagsState {
    fn new() -> Self {
        DeserializerTagsState {
            marker_tag_to_deserializer_tag: HashMap::new(),
            marker_type_functions_1_based: vec![],
        }
    }
}

/// Get or insert the deserializer tag for each marker type. The tag storage
/// is limited to 255 marker types. This is the same with the C++ side. It's
/// unlikely to reach to this limit, but if that's the case, C++ side needs
/// to change the uint8_t type for the deserializer tag as well.
pub fn get_or_insert_deserializer_tag<T>() -> u8
where
    T: ProfilerMarker,
{
    let unique_marker_tag = &T::marker_type_name as *const _ as usize;
    let mut state = DESERIALIZER_TAGS_STATE.write().unwrap();

    match state.marker_tag_to_deserializer_tag.get(&unique_marker_tag) {
        None => {
            // It's impossible to have length more than u8.
            let deserializer_tag = state.marker_type_functions_1_based.len() as u8 + 1;
            debug_assert!(
                deserializer_tag < 250,
                "Too many rust marker payload types! Please consider increasing the profiler \
                 buffer tag size."
            );

            state
                .marker_tag_to_deserializer_tag
                .insert(unique_marker_tag, deserializer_tag);
            state
                .marker_type_functions_1_based
                .push(MarkerTypeFunctions {
                    marker_type_name_fn: T::marker_type_name,
                    marker_type_display_fn: T::marker_type_display,
                    transmute_and_stream_fn: transmute_and_stream::<T>,
                });
            deserializer_tag
        }
        Some(deserializer_tag) => *deserializer_tag,
    }
}

/// A guard that will be used by the marker FFI functions for getting marker type functions.
pub struct MarkerTypeFunctionsReadGuard {
    guard: RwLockReadGuard<'static, DeserializerTagsState>,
}

impl MarkerTypeFunctionsReadGuard {
    pub fn iter<'a>(&'a self) -> impl Iterator<Item = &'a MarkerTypeFunctions> {
        self.guard.marker_type_functions_1_based.iter()
    }

    pub fn get<'a>(&'a self, deserializer_tag: u8) -> &'a MarkerTypeFunctions {
        self.guard
            .marker_type_functions_1_based
            .get(deserializer_tag as usize - 1)
            .expect("Failed to find the marker type functions for given deserializer tag")
    }
}

/// Locks the DESERIALIZER_TAGS_STATE and returns the marker type functions read guard.
pub fn get_marker_type_functions_read_guard() -> MarkerTypeFunctionsReadGuard {
    MarkerTypeFunctionsReadGuard {
        guard: DESERIALIZER_TAGS_STATE.read().unwrap(),
    }
}
