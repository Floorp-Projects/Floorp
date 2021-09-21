/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

//! [`MarkerSchema`] and other enums that will be used by `MarkerSchema`.

use crate::gecko_bindings::{bindings, structs::mozilla};
use std::mem::MaybeUninit;
use std::ops::DerefMut;
use std::os::raw::c_char;
use std::pin::Pin;

/// Marker locations to be displayed in the profiler front-end.
pub type Location = mozilla::MarkerSchema_Location;

/// Formats of marker properties for profiler front-end.
pub type Format = mozilla::MarkerSchema_Format;

/// Whether it's searchable or not in the profiler front-end.
pub type Searchable = mozilla::MarkerSchema_Searchable;

/// This object collects all the information necessary to stream the JSON schema
/// that informs the front-end how to display a type of markers.
/// It will be created and populated in `marker_type_display()` functions in each
/// marker type definition, see add/set functions.
///
/// It's a RAII object that constructs and destroys a C++ MarkerSchema object
/// pointed to a specified reference.
pub struct MarkerSchema {
    pub(crate) pin: Pin<Box<MaybeUninit<mozilla::MarkerSchema>>>,
}

impl MarkerSchema {
    // Initialize a marker schema with the given `Location`s.
    pub fn new(locations: &[Location]) -> Self {
        let mut marker_schema = Box::pin(std::mem::MaybeUninit::<mozilla::MarkerSchema>::uninit());

        unsafe {
            bindings::gecko_profiler_construct_marker_schema(
                marker_schema.deref_mut().as_mut_ptr(),
                locations.as_ptr(),
                locations.len(),
            );
        }
        MarkerSchema { pin: marker_schema }
    }

    /// Marker schema for types that have special frontend handling.
    /// Nothing else should be set in this case.
    pub fn new_with_special_frontend_location() -> Self {
        let mut marker_schema = Box::pin(std::mem::MaybeUninit::<mozilla::MarkerSchema>::uninit());
        unsafe {
            bindings::gecko_profiler_construct_marker_schema_with_special_front_end_location(
                marker_schema.deref_mut().as_mut_ptr(),
            );
        }
        MarkerSchema { pin: marker_schema }
    }

    /// Optional label in the marker chart.
    /// If not provided, the marker "name" will be used. The given string
    /// can contain element keys in braces to include data elements streamed by
    /// `stream_json_marker_data()`. E.g.: "This is {marker.data.text}"
    pub fn set_chart_label(&mut self, label: &str) -> &mut Self {
        unsafe {
            bindings::gecko_profiler_marker_schema_set_chart_label(
                self.pin.deref_mut().as_mut_ptr(),
                label.as_ptr() as *const c_char,
                label.len(),
            );
        }
        self
    }

    /// Optional label in the marker chart tooltip.
    /// If not provided, the marker "name" will be used. The given string
    /// can contain element keys in braces to include data elements streamed by
    /// `stream_json_marker_data()`. E.g.: "This is {marker.data.text}"
    pub fn set_tooltip_label(&mut self, label: &str) -> &mut Self {
        unsafe {
            bindings::gecko_profiler_marker_schema_set_tooltip_label(
                self.pin.deref_mut().as_mut_ptr(),
                label.as_ptr() as *const c_char,
                label.len(),
            );
        }
        self
    }

    /// Optional label in the marker table.
    /// If not provided, the marker "name" will be used. The given string
    /// can contain element keys in braces to include data elements streamed by
    /// `stream_json_marker_data()`. E.g.: "This is {marker.data.text}"
    pub fn set_table_label(&mut self, label: &str) -> &mut Self {
        unsafe {
            bindings::gecko_profiler_marker_schema_set_table_label(
                self.pin.deref_mut().as_mut_ptr(),
                label.as_ptr() as *const c_char,
                label.len(),
            );
        }
        self
    }

    /// Set all marker chart / marker tooltip / marker table labels with the same text.
    /// Same as the individual methods, the given string can contain element keys
    /// in braces to include data elements streamed by `stream_json_marker_data()`.
    /// E.g.: "This is {marker.data.text}"
    pub fn set_all_labels(&mut self, label: &str) -> &mut Self {
        unsafe {
            bindings::gecko_profiler_marker_schema_set_all_labels(
                self.pin.deref_mut().as_mut_ptr(),
                label.as_ptr() as *const c_char,
                label.len(),
            );
        }
        self
    }

    // Each data element that is streamed by `stream_json_marker_data()` can be
    // displayed as indicated by using one of the `add_...` function below.
    // Each `add...` will add a line in the full marker description. Parameters:
    // - `key`: Element property name as streamed by `stream_json_marker_data()`.
    // - `label`: Optional label. Defaults to the key name.
    // - `format`: How to format the data element value, see `Format` above.
    // - `searchable`: Optional, indicates if the value is used in searches,
    //   defaults to false.

    /// Add a key / format row for the marker data element.
    /// - `key`: Element property name as streamed by `stream_json_marker_data()`.
    /// - `format`: How to format the data element value, see `Format` above.
    pub fn add_key_format(&mut self, key: &str, format: Format) -> &mut Self {
        unsafe {
            bindings::gecko_profiler_marker_schema_add_key_format(
                self.pin.deref_mut().as_mut_ptr(),
                key.as_ptr() as *const c_char,
                key.len(),
                format,
            );
        }
        self
    }

    /// Add a key / label / format row for the marker data element.
    /// - `key`: Element property name as streamed by `stream_json_marker_data()`.
    /// - `label`: Optional label. Defaults to the key name.
    /// - `format`: How to format the data element value, see `Format` above.
    pub fn add_key_label_format(&mut self, key: &str, label: &str, format: Format) -> &mut Self {
        unsafe {
            bindings::gecko_profiler_marker_schema_add_key_label_format(
                self.pin.deref_mut().as_mut_ptr(),
                key.as_ptr() as *const c_char,
                key.len(),
                label.as_ptr() as *const c_char,
                label.len(),
                format,
            );
        }
        self
    }

    /// Add a key / format / searchable row for the marker data element.
    /// - `key`: Element property name as streamed by `stream_json_marker_data()`.
    /// - `format`: How to format the data element value, see `Format` above.
    pub fn add_key_format_searchable(
        &mut self,
        key: &str,
        format: Format,
        searchable: Searchable,
    ) -> &mut Self {
        unsafe {
            bindings::gecko_profiler_marker_schema_add_key_format_searchable(
                self.pin.deref_mut().as_mut_ptr(),
                key.as_ptr() as *const c_char,
                key.len(),
                format,
                searchable,
            );
        }
        self
    }

    /// Add a key / label / format / searchable row for the marker data element.
    /// - `key`: Element property name as streamed by `stream_json_marker_data()`.
    /// - `label`: Optional label. Defaults to the key name.
    /// - `format`: How to format the data element value, see `Format` above.
    /// - `searchable`: Optional, indicates if the value is used in searches,
    ///   defaults to false.
    pub fn add_key_label_format_searchable(
        &mut self,
        key: &str,
        label: &str,
        format: Format,
        searchable: Searchable,
    ) -> &mut Self {
        unsafe {
            bindings::gecko_profiler_marker_schema_add_key_label_format_searchable(
                self.pin.deref_mut().as_mut_ptr(),
                key.as_ptr() as *const c_char,
                key.len(),
                label.as_ptr() as *const c_char,
                label.len(),
                format,
                searchable,
            );
        }
        self
    }

    /// Add a key / value static row.
    /// - `key`: Element property name as streamed by `stream_json_marker_data()`.
    /// - `value`: Static value to display.
    pub fn add_static_label_value(&mut self, label: &str, value: &str) -> &mut Self {
        unsafe {
            bindings::gecko_profiler_marker_schema_add_static_label_value(
                self.pin.deref_mut().as_mut_ptr(),
                label.as_ptr() as *const c_char,
                label.len(),
                value.as_ptr() as *const c_char,
                value.len(),
            );
        }
        self
    }
}

impl Drop for MarkerSchema {
    fn drop(&mut self) {
        unsafe {
            bindings::gecko_profiler_destruct_marker_schema(self.pin.deref_mut().as_mut_ptr());
        }
    }
}
