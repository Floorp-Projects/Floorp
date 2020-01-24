//! Utilities for interacting with the generated Emscripten module.
//! Most functionality is generalized, but some functionality is specific to SPIRV-Cross.

use crate::{bindings, ErrorCode};
use js_sys::{global, Object, Reflect, Uint32Array, Uint8Array};
use wasm_bindgen::prelude::*;

#[wasm_bindgen]
extern "C" {
    // Raw Emscripten bindings
    #[wasm_bindgen(js_namespace = sc_internal)]
    fn _malloc(size: u32) -> u32;

    #[wasm_bindgen(js_namespace = sc_internal)]
    fn _free(offset: u32);
}

pub fn get_module() -> Module {
    const MODULE_NAME: &'static str = "sc_internal";
    let module = Reflect::get(&global(), &JsValue::from_str(MODULE_NAME))
        .unwrap()
        .into();
    Module { module }
}

const U32_SIZE: u32 = std::mem::size_of::<u32>() as u32;

fn get_value<T>(object: &Object, key: &str) -> T
where
    T: std::convert::From<wasm_bindgen::JsValue>,
{
    Reflect::get(object, &JsValue::from_str(key))
        .unwrap()
        .into()
}

/// An Emscripten pointer.
/// Internally stores an offset to a location on the Emscripten `u8` heap.
#[derive(Clone, Copy)]
pub struct Pointer {
    offset: u32,
}

impl Pointer {
    pub fn from_offset(offset: u32) -> Self {
        Pointer { offset }
    }

    pub fn as_offset(&self) -> u32 {
        self.offset
    }
}

pub struct Module {
    module: Object,
}

impl Module {
    /// Allocate memory on the heap.
    pub unsafe fn allocate(&self, byte_len: u32) -> Pointer {
        Pointer {
            offset: _malloc(byte_len),
        }
    }

    pub unsafe fn free(&self, pointer: Pointer) {
        _free(pointer.as_offset())
    }

    // Read a `u32` value from the heap.
    pub unsafe fn get_u32(&self, pointer: Pointer) -> u32 {
        let offset = &JsValue::from_f64((pointer.offset / U32_SIZE) as f64);
        // TODO: Remove Reflect
        Reflect::get(&self.heap_u32(), offset)
            .unwrap()
            .as_f64()
            .unwrap() as u32
    }

    /// Set memory on the heap to `bytes`.
    pub unsafe fn set_from_u8_typed_array(&self, pointer: Pointer, bytes: Uint8Array) {
        let buffer: JsValue = self.heap_u8().buffer().into();
        let memory =
            Uint8Array::new_with_byte_offset_and_length(&buffer, pointer.offset, bytes.length());
        memory.set(&bytes, 0);
    }

    /// Set memory on the heap to `bytes`.
    pub unsafe fn set_from_u8_slice(&self, pointer: Pointer, bytes: &[u8]) {
        self.set_from_u8_typed_array(pointer, Uint8Array::view(bytes));
    }

    fn heap_u8(&self) -> Uint8Array {
        const HEAP_U8: &'static str = "HEAPU8";
        get_value(&self.module, HEAP_U8)
    }

    fn heap_u32(&self) -> Uint32Array {
        const HEAP_U32: &'static str = "HEAPU32";
        get_value(&self.module, HEAP_U32)
    }

    /// Clones all bytes from the heap into a `Vec<u8>` while `should_continue` returns `true`.
    /// Optionally include the last byte (i.e. to support peeking the final byte for nul-terminated strings).
    pub unsafe fn read_bytes_into_vec_while<F>(
        &self,
        pointer: Pointer,
        should_continue: F,
        include_last_byte: bool,
    ) -> Vec<u8>
    where
        F: Fn(u8, usize) -> bool,
    {
        let mut bytes = Vec::new();
        let heap = &self.heap_u8();
        let start_offset = pointer.offset as usize;
        loop {
            let bytes_read = bytes.len();
            let offset = &JsValue::from_f64((start_offset + bytes_read) as f64);
            let byte = Reflect::get(heap, offset).unwrap().as_f64().unwrap() as u8;
            if should_continue(byte, bytes_read) {
                bytes.push(byte);
                continue;
            }
            if include_last_byte {
                bytes.push(byte);
            }
            break;
        }
        bytes
    }

    /// Clones all bytes from the heap into the pointer provided while `should_continue` returns `true`.
    /// Optionally include the last byte (i.e. to support peeking the final byte for nul-terminated strings).
    /// Assumes the memory at the pointer is large enough to hold all bytes read (based on when `should_continue` terminates).
    pub unsafe fn read_bytes_into_pointer_while<F>(
        &self,
        pointer: Pointer,
        should_continue: F,
        include_last_byte: bool,
        into_pointer: *mut u8,
    ) where
        F: Fn(u8, usize) -> bool,
    {
        let heap = &self.heap_u8();
        let start_offset = pointer.offset as usize;
        let mut bytes_read = 0;
        loop {
            let offset = &JsValue::from_f64((start_offset + bytes_read) as f64);
            let byte = Reflect::get(heap, offset).unwrap().as_f64().unwrap() as u8;
            if should_continue(byte, bytes_read) {
                *into_pointer.offset(bytes_read as isize) = byte;
                bytes_read += 1;
                continue;
            }
            if include_last_byte {
                *into_pointer.offset(bytes_read as isize) = byte;
            }
            break;
        }
    }
}
