/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This module contains a wrapper around the prefs_parser that allows it to be
//! more easily used to extract a single pref value from a file.

use prefs_parser::{prefs_parser_parse, PrefType, PrefValue, PrefValueKind};
use std::{
    cell::Cell,
    ffi::{CStr, CString},
    fs::File,
    io::Read,
    os::raw::c_char,
};

thread_local! {
    static PREF_NAME_TO_READ: Cell<String> = Cell::new(String::new());
    static PREF_VALUE: Cell<Option<String>> = Cell::new(None);
    static ERROR_COUNT: Cell<u32> = Cell::new(0);
}

/// This function reads the file at the path specified and finds the value for the pref name
/// specified. The function is capable of reading a String type pref only.
///
/// The underlying mechanism for doing this is prefs_parser. Unfortunately, prefs_parser is made to
/// be convenient to call from C++ and is difficult to call from Rust. This function wraps that
/// interface.
pub fn read_pref(path: &str, pref_to_read: &str) -> Result<String, String> {
    PREF_NAME_TO_READ.with(|pref_value| pref_value.set(pref_to_read.to_owned()));
    PREF_VALUE.with(|pref_value| pref_value.set(None));
    ERROR_COUNT.with(|error_count| error_count.set(0));

    let mut file =
        File::open(path).map_err(|e| format!("Unable to open \"{}\" to read pref: {}", path, e))?;
    let mut data = String::new();
    file.read_to_string(&mut data)
        .map_err(|e| format!("Unable to read \"{}\": {}", path, e))?;

    let path_cstring =
        CString::new(path).map_err(|e| format!("Unable to convert path to cstring: {}", e))?;
    let data_cstring = CString::new(data).map_err(|e| {
        format!(
            "Unable to convert contents of \"{}\" to a cstring: {}",
            path, e
        )
    })?;

    let success = prefs_parser_parse(
        path_cstring.as_ptr(),
        PrefValueKind::Default,
        data_cstring.as_ptr(),
        data_cstring.as_bytes().len(),
        pref_fn,
        error_fn,
    );

    if !success {
        return Err("prefs_parser_parse returned false".to_string());
    }
    let error_count = ERROR_COUNT.with(|error_count| error_count.get());
    if error_count > 0 {
        return Err(format!("Encountered {} errors while parsing", error_count));
    }
    let pref_value = PREF_VALUE.with(|pref_value| pref_value.take());
    pref_value.ok_or_else(|| format!("Pref name \"{}\" not found", pref_to_read))
}

/// This function is crafted to be compatible with prefs_parser::PrefFn.
/// It accepts pref data and searches for a string pref that with a name matching the one stored in
/// PREF_NAME_TO_READ. When it finds a matching pref, it stores its value in PREF_VALUE.
extern "C" fn pref_fn(
    pref_name: *const c_char,
    pref_type: PrefType,
    _pref_value_kind: PrefValueKind,
    pref_value: PrefValue,
    _is_sticky: bool,
    _is_locked: bool,
) {
    if pref_type != PrefType::String {
        return;
    }
    let pref_name_str = match unsafe { CStr::from_ptr(pref_name) }.to_str() {
        Ok(value) => value,
        Err(_) => return,
    };
    let pref_matches = PREF_NAME_TO_READ.with(|pref_cell| -> bool {
        let pref_to_read = pref_cell.take();
        let pref_matches = pref_name_str == pref_to_read;
        pref_cell.set(pref_to_read);
        pref_matches
    });
    if !pref_matches {
        return;
    }
    let pref_value_str = match unsafe { CStr::from_ptr(pref_value.string_val) }.to_str() {
        Ok(value) => value,
        Err(_) => return,
    };
    PREF_VALUE.with(|pref_value| pref_value.set(Some(pref_value_str.to_owned())));
}

/// This function is crafted to be compatible with prefs_parser::ErrorFn.
/// When an error is passed to it, it increments ERROR_COUNT.
extern "C" fn error_fn(_msg: *const c_char) {
    ERROR_COUNT.with(|error_cell| {
        let prev_error_count = error_cell.get();
        error_cell.set(prev_error_count + 1);
    });
}
