/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Gecko JSON writer support for marker API.

use crate::gecko_bindings::{bindings, structs::mozilla};
use std::os::raw::c_char;

/// Wrapper for the C++ SpliceableJSONWriter object. It exposes some methods to
/// add various properties to the JSON.
#[derive(Debug)]
pub struct JSONWriter<'a>(&'a mut mozilla::baseprofiler::SpliceableJSONWriter);

impl<'a> JSONWriter<'a> {
    /// Constructor for the JSONWriter object. It takes a C++ SpliceableJSONWriter
    /// reference as its argument and stores it for later accesses.
    pub(crate) fn new(json_writer: &'a mut mozilla::baseprofiler::SpliceableJSONWriter) -> Self {
        JSONWriter(json_writer)
    }

    /// Adds an int property to the JSON.
    /// Prints: "<name>": <value>
    pub fn int_property(&mut self, name: &str, value: i64) {
        unsafe {
            bindings::gecko_profiler_json_writer_int_property(
                self.0,
                name.as_ptr() as *const c_char,
                name.len(),
                value,
            );
        }
    }

    /// Adds a float property to the JSON.
    /// Prints: "<name>": <value>
    pub fn float_property(&mut self, name: &str, value: f64) {
        unsafe {
            bindings::gecko_profiler_json_writer_float_property(
                self.0,
                name.as_ptr() as *const c_char,
                name.len(),
                value,
            );
        }
    }

    /// Adds an bool property to the JSON.
    /// Prints: "<name>": <value>
    pub fn bool_property(&mut self, name: &str, value: bool) {
        unsafe {
            bindings::gecko_profiler_json_writer_bool_property(
                self.0,
                name.as_ptr() as *const c_char,
                name.len(),
                value,
            );
        }
    }

    /// Adds a string property to the JSON.
    /// Prints: "<name>": "<value>"
    pub fn string_property(&mut self, name: &str, value: &str) {
        unsafe {
            bindings::gecko_profiler_json_writer_string_property(
                self.0,
                name.as_ptr() as *const c_char,
                name.len(),
                value.as_ptr() as *const c_char,
                value.len(),
            );
        }
    }

    /// Adds a unique string property to the JSON.
    /// Prints: "<name>": <string_table_index>
    pub fn unique_string_property(&mut self, name: &str, value: &str) {
        unsafe {
            bindings::gecko_profiler_json_writer_unique_string_property(
                self.0,
                name.as_ptr() as *const c_char,
                name.len(),
                value.as_ptr() as *const c_char,
                value.len(),
            );
        }
    }

    /// Adds a null property to the JSON.
    /// Prints: "<name>": null
    pub fn null_property(&mut self, name: &str) {
        unsafe {
            bindings::gecko_profiler_json_writer_null_property(
                self.0,
                name.as_ptr() as *const c_char,
                name.len(),
            );
        }
    }
}
