/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

//! ## Gecko profiler marker support
//!
//! This marker API has a few different functions that you can use to mark a part of your code.
//! There are three main marker functions to use from Rust: [`add_untyped_marker`],
//! [`add_text_marker`] and [`add_marker`]. They are similar to what we have on
//! the C++ side. Please take a look at the marker documentation in the Firefox
//! source docs to learn more about them:
//! https://firefox-source-docs.mozilla.org/tools/profiler/markers-guide.html
//!
//! ### Simple marker without any additional data
//!
//! The simplest way to add a marker without any additional information is the
//! [`add_untyped_marker`] API. You can use it to mark a part of the code with
//! only a name. E.g.:
//!
//! ```
//! gecko_profiler::add_untyped_marker(
//!     // Name of the marker as a string.
//!     "Marker Name",
//!     // Category with an optional sub-category.
//!     gecko_profiler_category!(Graphics, DisplayListBuilding),
//!     // MarkerOptions that keeps options like marker timing and marker stack.
//!     Default::default(),
//! );
//! ```
//!
//! Please see the [`gecko_profiler_category!`], [`MarkerOptions`],[`MarkerTiming`]
//! and [`MarkerStack`] to learn more about these.
//!
//! You can also give explicit [`MarkerOptions`] value like these:
//!
//! ```
//! // With both timing and stack fields:
//! MarkerOptions { timing: MarkerTiming::instant_now(), stack: MarkerStack::Full }
//! // Or with some fields as default:
//! MarkerOptions { timing: MarkerTiming::instant_now(), ..Default::default() }
//! ```
//!
//! ### Marker with only an additional text for more information:
//!
//! The next and slightly more advanced API is [`add_text_marker`].
//! This is used to add a marker name + a string value for extra information.
//! E.g.:
//!
//! ```
//! let info = "info about this marker";
//! ...
//! gecko_profiler::add_text_marker(
//!     // Name of the marker as a string.
//!     "Marker Name",
//!     // Category with an optional sub-category.
//!     gecko_profiler_category!(DOM),
//!     // MarkerOptions that keeps options like marker timing and marker stack.
//!     MarkerOptions {
//!         timing: MarkerTiming::instant_now(),
//!         ..Default::default()
//!     },
//!     // Additional information as a string.
//!     info,
//! );
//! ```
//!
//! ### Marker with a more complex payload and different visualization in the profiler front-end.
//!
//! [`add_marker`] is the most advanced API that you can use to add different types
//! of values as data to your marker and customize the visualization of that marker
//! in the profiler front-end (profiler.firefox.com).
//!
//! To be able to add a a marker, first you need to create your marker payload
//! struct in your codebase and implement the [`ProfilerMarker`] trait like this:
//!
//! ```
//! #[derive(Serialize, Deserialize, Debug)]
//! pub struct TestMarker {
//!     a: u32,
//!     b: String,
//! }
//!
//! // Please see the documentation of [`ProfilerMarker`].
//! impl gecko_profiler::ProfilerMarker for TestMarker {
//!     fn marker_type_name() -> &'static str {
//!         "marker type from rust"
//!     }
//!     fn marker_type_display() -> gecko_profiler::MarkerSchema {
//!         use gecko_profiler::marker::schema::*;
//!         let mut schema = MarkerSchema::new(&[Location::MarkerChart]);
//!         schema.set_chart_label("Name: {marker.name}");
//!         schema.set_tooltip_label("{marker.data.a}");
//!         schema.add_key_label_format("a", "A Value", Format::Integer);
//!         schema.add_key_label_format("b", "B Value", Format::String);
//!         schema
//!     }
//!     fn stream_json_marker_data(&self, json_writer: &mut gecko_profiler::JSONWriter) {
//!         json_writer.int_property("a", self.a.into());
//!         json_writer.string_property("b", &self.b);
//!     }
//! }
//! ```
//!
//! Once you've created this payload and implemented the [`ProfilerMarker`], you
//! can now add this marker in the code that you would like to measure. E.g.:
//!
//! ```
//! gecko_profiler::add_marker(
//!     // Name of the marker as a string.
//!     "Marker Name",
//!     // Category with an optional sub-category.
//!     gecko_profiler_category!(Graphics, DisplayListBuilding),
//!     // MarkerOptions that keeps options like marker timing and marker stack.
//!     Default::default(),
//!     // Marker payload.
//!     TestMarker {a: 12, b: "hello".to_owned()},
//! );
//! ```

pub(crate) mod deserializer_tags_state;
pub mod options;
pub mod schema;

pub use options::*;
pub use schema::MarkerSchema;

use crate::gecko_bindings::{bindings, profiling_categories::ProfilingCategoryPair};
use crate::json_writer::JSONWriter;
use crate::marker::deserializer_tags_state::get_or_insert_deserializer_tag;
use serde::{de::DeserializeOwned, Deserialize, Serialize};
use std::os::raw::c_char;

/// Marker API to add a new simple marker without any payload.
/// Please see the module documentation on how to add a marker with this API.
pub fn add_untyped_marker(name: &str, category: ProfilingCategoryPair, mut options: MarkerOptions) {
    if !crate::profiler_state::can_accept_markers() {
        // Nothing to do.
        return;
    }

    unsafe {
        bindings::gecko_profiler_add_marker_untyped(
            name.as_ptr() as *const c_char,
            name.len(),
            category.to_cpp_enum_value(),
            options.timing.0.as_mut_ptr(),
            options.stack,
        )
    }
}

/// Marker API to add a new marker with additional text for details.
/// Please see the module documentation on how to add a marker with this API.
pub fn add_text_marker(
    name: &str,
    category: ProfilingCategoryPair,
    mut options: MarkerOptions,
    text: &str,
) {
    if !crate::profiler_state::can_accept_markers() {
        // Nothing to do.
        return;
    }

    unsafe {
        bindings::gecko_profiler_add_marker_text(
            name.as_ptr() as *const c_char,
            name.len(),
            category.to_cpp_enum_value(),
            options.timing.0.as_mut_ptr(),
            options.stack,
            text.as_ptr() as *const c_char,
            text.len(),
        )
    }
}

/// Trait that every profiler marker payload struct needs to implement.
/// This will tell the profiler back-end how to serialize it as json and
/// the front-end how to display the marker.
/// Please also see the documentation here:
/// https://firefox-source-docs.mozilla.org/tools/profiler/markers-guide.html#how-to-define-new-marker-types
///
/// - `marker_type_name`: Returns a static string as the marker type name. This
/// should be unique and it is used to keep track of the type of markers in the
/// profiler storage, and to identify them uniquely on the profiler front-end.
/// - `marker_type_display`: Where and how to display the marker and its data.
/// Returns a `MarkerSchema` object which will be forwarded to the profiler
/// front-end.
/// - `stream_json_marker_data`: Data specific to this marker type should be
/// serialized to JSON for the profiler front-end. All the common marker data
/// like marker name, category, timing will be serialized automatically. But
/// marker specific data should be serialized here.
pub trait ProfilerMarker: Serialize + DeserializeOwned {
    /// A static method that returns the name of the marker type.
    fn marker_type_name() -> &'static str;
    /// A static method that returns a `MarkerSchema`, which contains all the
    /// information needed to stream the display schema associated with a
    /// marker type.
    fn marker_type_display() -> MarkerSchema;
    /// A method that streams the marker payload data as JSON object properties.
    /// Please see the [JSONWriter] struct to see its methods.
    fn stream_json_marker_data(&self, json_writer: &mut JSONWriter);
}

/// A function that deserializes the marker payload and streams it to the JSON.
unsafe fn transmute_and_stream<T>(
    payload: *const u8,
    payload_size: usize,
    json_writer: &mut JSONWriter,
) where
    T: ProfilerMarker,
{
    let payload_slice = std::slice::from_raw_parts(payload, payload_size);
    let payload: T = bincode::deserialize(&payload_slice).unwrap();
    payload.stream_json_marker_data(json_writer);
}

/// Main marker API to add a new marker to profiler buffer.
/// Please see the module documentation on how to add a marker with this API.
pub fn add_marker<T>(
    name: &str,
    category: ProfilingCategoryPair,
    mut options: MarkerOptions,
    payload: T,
) where
    T: ProfilerMarker,
{
    if !crate::profiler_state::can_accept_markers() {
        // Nothing to do.
        return;
    }

    let encoded_payload: Vec<u8> = bincode::serialize(&payload).unwrap();
    let payload_size = encoded_payload.len();
    let maker_tag = get_or_insert_deserializer_tag::<T>();

    unsafe {
        bindings::gecko_profiler_add_marker(
            name.as_ptr() as *const c_char,
            name.len(),
            category.to_cpp_enum_value(),
            options.timing.0.as_mut_ptr(),
            options.stack,
            maker_tag,
            encoded_payload.as_ptr(),
            payload_size,
        )
    }
}

/// Tracing marker type for Rust code.
/// This must be kept in sync with the `mozilla::baseprofiler::markers::Tracing`
/// C++ counterpart.
#[derive(Serialize, Deserialize, Debug)]
pub struct Tracing(pub String);

impl ProfilerMarker for Tracing {
    fn marker_type_name() -> &'static str {
        "tracing"
    }

    fn stream_json_marker_data(&self, json_writer: &mut JSONWriter) {
        if self.0.len() != 0 {
            json_writer.string_property("category", &self.0);
        }
    }

    // Tracing marker is a bit special because we have the same schema in the
    // C++ side. This function will only get called when no Tracing markers are
    // generated from the C++ side. But, most of the time, this will not be called
    // when there is another C++ Tracing marker.
    fn marker_type_display() -> MarkerSchema {
        use crate::marker::schema::*;
        let mut schema = MarkerSchema::new(&[
            Location::MarkerChart,
            Location::MarkerTable,
            Location::TimelineOverview,
        ]);
        schema.add_key_label_format("category", "Type", Format::String);
        schema
    }
}
