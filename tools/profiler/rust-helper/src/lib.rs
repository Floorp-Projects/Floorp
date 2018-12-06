extern crate memmap;
extern crate thin_vec;

#[cfg(feature = "parse_elf")]
extern crate object;
#[cfg(feature = "parse_elf")]
extern crate goblin;

mod compact_symbol_table;

#[cfg(feature = "parse_elf")]
mod elf;

#[cfg(feature = "parse_elf")]
use memmap::MmapOptions;
#[cfg(feature = "parse_elf")]
use std::fs::File;

use std::ffi::CStr;
use std::os::raw::c_char;
use compact_symbol_table::CompactSymbolTable;

#[cfg(feature = "parse_elf")]
pub fn get_compact_symbol_table_from_file(
    debug_path: &str,
    breakpad_id: &str,
) -> Option<CompactSymbolTable> {
    let file = File::open(debug_path).ok()?;
    let buffer = unsafe { MmapOptions::new().map(&file).ok()? };
    elf::get_compact_symbol_table(&buffer, breakpad_id)
}

#[cfg(not(feature = "parse_elf"))]
pub fn get_compact_symbol_table_from_file(
    _debug_path: &str,
    _breakpad_id: &str,
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
    let breakpad_id = unsafe { CStr::from_ptr(breakpad_id).to_string_lossy() };

    match get_compact_symbol_table_from_file(&debug_path, &breakpad_id) {
        Some(mut st) => {
            std::mem::swap(symbol_table, &mut st);
            true
        }
        None => false,
    }
}
