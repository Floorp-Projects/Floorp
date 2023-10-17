/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use nsstring::nsCString;
use std::{ffi::c_void, sync::Mutex};

#[cfg(any(target_os = "linux", target_os = "android"))]
use std::arch::global_asm;

#[repr(C)]
#[derive(Copy, Clone)]
pub enum AnnotationContents {
    Empty,
    NSCString,
    CString,
    CharBuffer,
    USize,
    ByteBuffer(u32),
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Annotation {
    pub id: u32,
    pub contents: AnnotationContents,
    pub address: usize,
}

pub struct AnnotationTable {
    data: Vec<Annotation>,
    magic_number: u32,
}

impl AnnotationTable {
    const fn new() -> AnnotationTable {
        AnnotationTable {
            data: Vec::new(),
            magic_number: ANNOTATION_TYPE,
        }
    }

    pub const fn verify(&self) -> bool {
        self.magic_number == ANNOTATION_TYPE
    }

    pub fn get_ptr(&self) -> *const Annotation {
        self.data.as_ptr()
    }

    pub fn len(&self) -> usize {
        self.data.len()
    }
}

pub type AnnotationMutex = Mutex<AnnotationTable>;

#[cfg(target_os = "windows")]
#[link_section = "mozannot"]
static MOZANNOTATIONS: AnnotationMutex = Mutex::new(AnnotationTable::new());
#[cfg(any(target_os = "linux", target_os = "android"))]
static MOZANNOTATIONS: AnnotationMutex = Mutex::new(AnnotationTable::new());
#[cfg(target_os = "macos")]
#[link_section = "__DATA,mozannotation"]
static MOZANNOTATIONS: AnnotationMutex = Mutex::new(AnnotationTable::new());

#[no_mangle]
unsafe fn mozannotation_get() -> *const AnnotationMutex {
    &MOZANNOTATIONS as _
}

#[cfg(any(target_os = "linux", target_os = "android"))]
extern "C" {
    pub static MOZANNOTATION_NOTE_REFERENCE: &'static u32;
    pub static __ehdr_start: [u8; 0];
}

#[cfg(target_os = "windows")]
pub const ANNOTATION_SECTION: &'static [u8; 8] = b"mozannot";

// TODO: Use the following constants in the assembly below when constant
// expressions are stabilized: https://github.com/rust-lang/rust/issues/93332
#[cfg(any(target_os = "linux", target_os = "android"))]
const _ANNOTATION_NOTE_ALIGNMENT: u32 = 4;
#[cfg(any(target_os = "linux", target_os = "android"))]
pub const ANNOTATION_NOTE_NAME: &str = "mozannotation";
pub const ANNOTATION_TYPE: u32 = u32::from_le_bytes(*b"MOZA");

// We use the crashpad crash info trick here. We create a program note which
// we'll use to find the location of the MOZANNOTATIONS static. Since program
// headers are always available we'll always be able to find this note in the
// memory of the crashed program, even if it's stripped or the backing file on
// disk has been deleted.
//
// We'll set the note type and name so we can easily recognize it (see the
// constants above). In the note's desc field we'll have the linker store the
// offset between the address of the MOZANNOTATIONS static and the desc field
// itself.
//
// At runtime we'll localize the note in the target process' memory, find the
// address of the `desc` field, load its contents (that is the offset we stored
// at link time) and add them together. The resulting address is the location of
// the MOZANNOTATIONS static in memory.
//
// When elfhack is used, the note might be moved after the aforementioned offset
// is calculated, without it being updated. To compensate for this we store the
// offset between the `ehdr` field and the ELF header. At runtime we can
// use this offset to adjust for the shift of the `desc` field.
#[cfg(all(
    target_pointer_width = "64",
    any(target_os = "linux", target_os = "android")
))]
global_asm!(
    // The section holding the note will be called '.note.moz.annotation'. We
    // create a program note that's allocated ('a' option) in the target binary
    // so that it's loaded into memory.
    "  .section .note.moz.annotation,\"a\",%note",
    // Note alignment must be 4 bytes because that's the default alignment for
    // that section. If a different alignment is chosen the note will end up in
    // its own section which we don't want.
    "  .balign 4", // TODO: _ANNOTATION_NOTE_ALIGNMENT
    "MOZANNOTATION_NOTE:",
    "  .long name_end - name", // size in bytes of the note's name
    "  .long desc_end - desc", // size in bytes of the note's desc field
    "  .long 0x415a4f4d",      // TODO: _ANNOTATION_TYPE, MOZA in reverse
    "name:",
    "  .asciz \"mozannotation\"", // TODO: _ANNOTATION_NOTE_NAME
    "name_end:",
    "  .balign 4", // TODO: _ANNOTATION_NOTE_ALIGNMENT
    "desc:",
    "  .quad {mozannotation_symbol} - desc",
    "ehdr:",
    "  .quad {__ehdr_start} - ehdr",
    "desc_end:",
    "  .size MOZANNOTATION_NOTE, .-MOZANNOTATION_NOTE",
    mozannotation_symbol = sym MOZANNOTATIONS,
    __ehdr_start = sym __ehdr_start
);

// The following global_asm!() expressions for other targets because Rust's
// support for putting statements within expressions is still experimental-only.
// Once https://github.com/rust-lang/rust/issues/15701 is fixed this can be
// folded in the `global_asm!()` statement above.

#[cfg(all(
    target_pointer_width = "32",
    any(target_os = "linux", target_os = "android")
))]
global_asm!(
    "  .section .note.moz.annotation,\"a\",%note",
    "  .balign 4",
    "MOZANNOTATION_NOTE:",
    "  .long name_end - name",
    "  .long desc_end - desc",
    "  .long 0x415a4f4d",
    "name:",
    "  .asciz \"mozannotation\"",
    "name_end:",
    "  .balign 4",
    "desc:",
    "  .long {mozannotation_symbol} - desc",
    "ehdr:",
    "  .long {__ehdr_start} - ehdr",
    "desc_end:",
    "  .size MOZANNOTATION_NOTE, .-MOZANNOTATION_NOTE",
    mozannotation_symbol = sym MOZANNOTATIONS,
    __ehdr_start = sym __ehdr_start
);

#[cfg(all(
    any(target_os = "linux", target_os = "android"),
    not(target_arch = "arm")
))]
global_asm!(
    // MOZANNOTATION_NOTE can't be referenced directly because the relocation
    // used to make the reference may require that the address be 8-byte aligned
    // and notes must have 4-byte alignment.
    "  .section .rodata,\"a\",%progbits",
    "  .balign 8",
    // .globl indicates that it's available to link against other .o files.
    // .hidden indicates that it will not appear in the executable's symbol
    // table.
    "  .globl",
    "  .hidden MOZANNOTATION_NOTE_REFERENCE",
    "  .type MOZANNOTATION_NOTE_REFERENCE, %object",
    "MOZANNOTATION_NOTE_REFERENCE:",
    // The value of this quad isn't important. It exists to reference
    // MOZANNOTATION_NOTE, causing the linker to include the note into the
    // binary linking the mozannotation_client crate. The subtraction from name
    // is a convenience to allow the value to be computed statically.
    "  .quad name - MOZANNOTATION_NOTE",
);

// In theory we could have used the statement above for ARM targets but for some
// reason the current rust compiler rejects the .quad directive. As with the other
// duplicate code above we could replace this with a single conditional line once
// once https://github.com/rust-lang/rust/issues/15701 is fixed.

#[cfg(all(any(target_os = "linux", target_os = "android"), target_arch = "arm"))]
global_asm!(
    "  .section .rodata,\"a\",%progbits",
    "  .balign 8",
    "  .globl",
    "  .hidden MOZANNOTATION_NOTE_REFERENCE",
    "  .type MOZANNOTATION_NOTE_REFERENCE, %object",
    "MOZANNOTATION_NOTE_REFERENCE:",
    "  .long name - MOZANNOTATION_NOTE",
);

/// This structure mirrors the contents of the note declared above in the
/// assembly blocks. It is used to copy the contents of the note out of the
/// target process.
#[cfg(any(target_os = "linux", target_os = "android"))]
#[allow(dead_code)]
#[repr(C, packed(4))]
pub struct MozAnnotationNote {
    pub namesz: u32,
    pub descsz: u32,
    pub note_type: u32,
    pub name: [u8; 16], // "mozannotation" plus padding to next 4-bytes boundary
    pub desc: usize,
    pub ehdr: isize,
}

/// Register an annotation containing an nsCString.
/// Returns false if the annotation is already present (and doesn't change it).
///
/// This function will be exposed to C++
#[no_mangle]
pub extern "C" fn mozannotation_register_nscstring(id: u32, address: *const nsCString) -> bool {
    let annotations = &mut MOZANNOTATIONS.lock().unwrap().data;

    if annotations.iter().any(|e| e.id == id) {
        return false;
    }

    annotations.push(Annotation {
        id,
        contents: AnnotationContents::NSCString,
        address: address as usize,
    });

    true
}

/// Register an annotation containing a null-terminated string.
/// Returns false if the annotation is already present (and doesn't change it).
///
/// This function will be exposed to C++
#[no_mangle]
pub extern "C" fn mozannotation_register_cstring(
    id: u32,
    address: *const *const std::ffi::c_char,
) -> bool {
    let annotations = &mut MOZANNOTATIONS.lock().unwrap().data;

    if annotations.iter().any(|e| e.id == id) {
        return false;
    }

    annotations.push(Annotation {
        id,
        contents: AnnotationContents::CString,
        address: address as usize,
    });

    true
}

/// Register an annotation pointing to a buffer holding a null-terminated string.
/// Returns false if the annotation is already present (and doesn't change it).
///
/// This function will be exposed to C++
#[no_mangle]
pub extern "C" fn mozannotation_register_char_buffer(
    id: u32,
    address: *const std::ffi::c_char,
) -> bool {
    let annotations = &mut MOZANNOTATIONS.lock().unwrap().data;

    if annotations.iter().any(|e| e.id == id) {
        return false;
    }

    annotations.push(Annotation {
        id,
        contents: AnnotationContents::CharBuffer,
        address: address as usize,
    });

    true
}

/// Register an annotation pointing to a variable of type usize.
/// Returns false if the annotation is already present (and doesn't change it).
///
/// This function will be exposed to C++
#[no_mangle]
pub extern "C" fn mozannotation_register_usize(id: u32, address: *const usize) -> bool {
    let annotations = &mut MOZANNOTATIONS.lock().unwrap().data;

    if annotations.iter().any(|e| e.id == id) {
        return false;
    }

    annotations.push(Annotation {
        id,
        contents: AnnotationContents::USize,
        address: address as usize,
    });

    true
}

/// Register an annotation pointing to a fixed size buffer.
/// Returns false if the annotation is already present (and doesn't change it).
///
/// This function will be exposed to C++
#[no_mangle]
pub extern "C" fn mozannotation_register_bytebuffer(
    id: u32,
    address: *const c_void,
    size: u32,
) -> bool {
    let annotations = &mut MOZANNOTATIONS.lock().unwrap().data;

    if annotations.iter().any(|e| e.id == id) {
        return false;
    }

    annotations.push(Annotation {
        id,
        contents: AnnotationContents::ByteBuffer(size),
        address: address as usize,
    });

    true
}

/// Unregister a crash annotation. Returns false if the annotation is not present.
///
/// This function will be exposed to C++
#[no_mangle]
pub extern "C" fn mozannotation_unregister(id: u32) -> bool {
    let annotations = &mut MOZANNOTATIONS.lock().unwrap().data;
    let index = annotations.iter().position(|e| e.id == id);

    if let Some(index) = index {
        annotations.remove(index);
        return true;
    }

    false
}
