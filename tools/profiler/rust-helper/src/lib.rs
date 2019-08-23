/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate memmap;
extern crate rustc_demangle;
extern crate thin_vec;

#[cfg(feature = "parse_elf")]
extern crate goblin;
#[cfg(feature = "parse_elf")]
extern crate object;

mod compact_symbol_table;

#[cfg(feature = "parse_elf")]
mod elf;

#[cfg(feature = "parse_elf")]
use memmap::MmapOptions;
#[cfg(feature = "parse_elf")]
use std::fs::File;

use compact_symbol_table::CompactSymbolTable;
use rustc_demangle::try_demangle;
use std::ffi::CStr;
use std::mem;
use std::os::raw::c_char;
use std::ptr;

#[cfg(feature = "parse_elf")]
pub fn get_compact_symbol_table_from_file(
    debug_path: &str,
    breakpad_id: Option<&str>,
) -> Option<CompactSymbolTable> {
    let file = File::open(debug_path).ok()?;
    let buffer = unsafe { MmapOptions::new().map(&file).ok()? };
    elf::get_compact_symbol_table(&buffer, breakpad_id)
}

#[cfg(not(feature = "parse_elf"))]
pub fn get_compact_symbol_table_from_file(
    _debug_path: &str,
    _breakpad_id: Option<&str>,
) -> Option<CompactSymbolTable> {
    None
}

#[no_mangle]
pub extern "C" fn profiler_get_symbol_table(
    debug_path: *const c_char,
    breakpad_id: *const c_char,
    symbol_table: &mut CompactSymbolTable,
) -> bool {
    let debug_path = unsafe { CStr::from_ptr(debug_path).to_string_lossy() };
    let breakpad_id = if breakpad_id.is_null() {
        None
    } else {
        match unsafe { CStr::from_ptr(breakpad_id).to_str() } {
            Ok(s) => Some(s),
            Err(_) => return false,
        }
    };

    match get_compact_symbol_table_from_file(&debug_path, breakpad_id.map(|id| id.as_ref())) {
        Some(mut st) => {
            std::mem::swap(symbol_table, &mut st);
            true
        }
        None => false,
    }
}

#[no_mangle]
pub extern "C" fn profiler_demangle_rust(
    mangled: *const c_char,
    buffer: *mut c_char,
    buffer_len: usize,
) -> bool {
    assert!(!mangled.is_null());
    assert!(!buffer.is_null());

    if buffer_len == 0 {
        return false;
    }

    let buffer: *mut u8 = unsafe { mem::transmute(buffer) };
    let mangled = match unsafe { CStr::from_ptr(mangled).to_str() } {
        Ok(s) => s,
        Err(_) => return false,
    };

    match try_demangle(mangled) {
        Ok(demangled) => {
            let mut demangled = format!("{:#}", demangled);
            if !demangled.is_ascii() {
                return false;
            }
            demangled.truncate(buffer_len - 1);

            let bytes = demangled.as_bytes();
            unsafe {
                ptr::copy(bytes.as_ptr(), buffer, bytes.len());
                ptr::write(buffer.offset(bytes.len() as isize), 0);
            }
            true
        }
        Err(_) => false,
    }
}
