/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

mod errors;
mod process_reader;

use crate::errors::*;
use process_reader::ProcessReader;

use mozannotation_client::{Annotation, AnnotationContents, AnnotationMutex};
use nsstring::nsCString;
use std::cmp::min;
use std::iter::FromIterator;
use std::mem::size_of;
use std::ptr::null_mut;
use thin_vec::ThinVec;

#[repr(C)]
pub enum AnnotationData {
    Empty,
    UsizeData(usize),
    NSCStringData(nsCString),
    ByteBuffer(ThinVec<u8>),
}

#[repr(C)]
pub struct CAnnotation {
    id: u32,
    data: AnnotationData,
}

#[cfg(target_os = "windows")]
type ProcessHandle = winapi::shared::ntdef::HANDLE;
#[cfg(any(target_os = "linux", target_os = "android"))]
type ProcessHandle = libc::pid_t;
#[cfg(any(target_os = "macos"))]
type ProcessHandle = mach2::mach_types::task_t;

/// Return the annotations of a given process.
///
/// This function will be exposed to C++
#[no_mangle]
pub extern "C" fn mozannotation_retrieve(
    process: usize,
    max_annotations: usize,
) -> *mut ThinVec<CAnnotation> {
    let result = retrieve_annotations(process as _, max_annotations);
    match result {
        // Leak the object as it will be owned by the C++ code from now on
        Ok(annotations) => Box::into_raw(annotations) as *mut _,
        Err(_) => null_mut(),
    }
}

/// Free the annotations returned by `mozannotation_retrieve()`.
///
/// # Safety
///
/// `ptr` must contain the value returned by a call to
/// `mozannotation_retrieve()` and be called only once.
#[no_mangle]
pub unsafe extern "C" fn mozannotation_free(ptr: *mut ThinVec<CAnnotation>) {
    // The annotation vector will be automatically destroyed when the contents
    // of this box are automatically dropped at the end of the function.
    let _box = Box::from_raw(ptr);
}

pub fn retrieve_annotations(
    process: ProcessHandle,
    max_annotations: usize,
) -> Result<Box<ThinVec<CAnnotation>>, RetrievalError> {
    let reader = ProcessReader::new(process)?;
    let address = reader.find_annotations()?;

    let mut mutex = reader.copy_object_shallow::<AnnotationMutex>(address)?;
    let mutex = unsafe { mutex.assume_init_mut() };

    // TODO: we should clear the poison value here before getting the mutex
    // contents. Right now we have to fail if the mutex was poisoned.
    let annotation_table = mutex.get_mut().map_err(|_e| RetrievalError::InvalidData)?;

    if !annotation_table.verify() {
        return Err(RetrievalError::InvalidAnnotationTable);
    }

    let vec_pointer = annotation_table.get_ptr();
    let length = annotation_table.len();
    let mut annotations = ThinVec::<CAnnotation>::with_capacity(min(max_annotations, length));

    for i in 0..length {
        let annotation_address = unsafe { vec_pointer.add(i) };
        if let Ok(annotation) = read_annotation(&reader, annotation_address as usize) {
            annotations.push(annotation);
        }
    }

    Ok(Box::new(annotations))
}

// Read an annotation from the given address
fn read_annotation(reader: &ProcessReader, address: usize) -> Result<CAnnotation, ReadError> {
    let raw_annotation = reader.copy_object::<Annotation>(address)?;
    let mut annotation = CAnnotation {
        id: raw_annotation.id,
        data: AnnotationData::Empty,
    };

    match raw_annotation.contents {
        AnnotationContents::Empty => {}
        AnnotationContents::NSCString => {
            let string = copy_nscstring(reader, raw_annotation.address)?;
            annotation.data = AnnotationData::NSCStringData(string);
        }
        AnnotationContents::CString => {
            let string = copy_null_terminated_string_pointer(reader, raw_annotation.address)?;
            annotation.data = AnnotationData::NSCStringData(string);
        }
        AnnotationContents::CharBuffer => {
            let string = copy_null_terminated_string(reader, raw_annotation.address)?;
            annotation.data = AnnotationData::NSCStringData(string);
        }
        AnnotationContents::USize => {
            let value = reader.copy_object::<usize>(raw_annotation.address)?;
            annotation.data = AnnotationData::UsizeData(value);
        }
        AnnotationContents::ByteBuffer(size) => {
            let value = copy_bytebuffer(reader, raw_annotation.address, size)?;
            annotation.data = AnnotationData::ByteBuffer(value);
        }
    };

    Ok(annotation)
}

fn copy_null_terminated_string_pointer(
    reader: &ProcessReader,
    address: usize,
) -> Result<nsCString, ReadError> {
    let buffer_address = reader.copy_object::<usize>(address)?;
    copy_null_terminated_string(reader, buffer_address)
}

fn copy_null_terminated_string(
    reader: &ProcessReader,
    address: usize,
) -> Result<nsCString, ReadError> {
    // Try copying the string word-by-word first, this is considerably faster
    // than one byte at a time.
    if let Ok(string) = copy_null_terminated_string_word_by_word(reader, address) {
        return Ok(string);
    }

    // Reading the string one word at a time failed, let's try again one byte
    // at a time. It's slow but it might work in situations where the string
    // alignment causes word-by-word access to straddle page boundaries.
    let mut length = 0;
    let mut string = Vec::<u8>::new();

    loop {
        let char = reader.copy_object::<u8>(address + length)?;
        length += 1;
        string.push(char);

        if char == 0 {
            break;
        }
    }

    Ok(nsCString::from(&string[..length]))
}

fn copy_null_terminated_string_word_by_word(
    reader: &ProcessReader,
    address: usize,
) -> Result<nsCString, ReadError> {
    const WORD_SIZE: usize = size_of::<usize>();
    let mut length = 0;
    let mut string = Vec::<u8>::new();

    loop {
        let mut array = reader.copy_array::<u8>(address + length, WORD_SIZE)?;
        let null_terminator = array.iter().position(|&e| e == 0);
        length += null_terminator.unwrap_or(WORD_SIZE);
        string.append(&mut array);

        if null_terminator.is_some() {
            break;
        }
    }

    Ok(nsCString::from(&string[..length]))
}

fn copy_nscstring(reader: &ProcessReader, address: usize) -> Result<nsCString, ReadError> {
    // HACK: This assumes the layout of the string
    let length_address = address + size_of::<usize>();
    let length = reader.copy_object::<u32>(length_address)?;

    if length > 0 {
        let data_address = reader.copy_object::<usize>(address)?;
        reader
            .copy_array::<u8>(data_address, length as _)
            .map(nsCString::from)
    } else {
        Ok(nsCString::new())
    }
}

fn copy_bytebuffer(
    reader: &ProcessReader,
    address: usize,
    size: u32,
) -> Result<ThinVec<u8>, ReadError> {
    let value = reader.copy_array::<u8>(address, size as _)?;
    Ok(ThinVec::<u8>::from_iter(value.into_iter()))
}
