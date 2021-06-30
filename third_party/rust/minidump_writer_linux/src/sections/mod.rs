pub mod app_memory;
pub mod exception_stream;
pub mod mappings;
pub mod memory_list_stream;
pub mod systeminfo_stream;
pub mod thread_list_stream;
pub mod thread_names_stream;

use crate::errors::MemoryWriterError;
use crate::minidump_format::*;
use std::convert::TryInto;
use std::io::{Cursor, Write};

type Result<T> = std::result::Result<T, MemoryWriterError>;

#[derive(Debug, PartialEq)]
pub struct MemoryWriter<T: Default + Sized> {
    pub position: MDRVA,
    pub size: usize,
    phantom: std::marker::PhantomData<T>,
}

impl<T> MemoryWriter<T>
where
    T: Default + Sized,
{
    /// Create a slot for a type T in the buffer, we can fill right now with real values.
    pub fn alloc_with_val(buffer: &mut Cursor<Vec<u8>>, val: T) -> Result<Self> {
        // Get position of this value (e.g. before we add ourselves there)
        let position = buffer.position();
        let size = std::mem::size_of::<T>();
        let bytes = unsafe { std::slice::from_raw_parts(&val as *const T as *const u8, size) };
        buffer.write_all(bytes)?;

        Ok(MemoryWriter {
            position: position as u32,
            size,
            phantom: std::marker::PhantomData::<T> {},
        })
    }

    /// Create a slot for a type T in the buffer, we can fill later with real values.
    /// This function fills it with `Default::default()`, which is less performant than
    /// using uninitialized memory, but safe.
    pub fn alloc(buffer: &mut Cursor<Vec<u8>>) -> Result<Self> {
        // Filling out the buffer with default-values
        let val: T = Default::default();
        Self::alloc_with_val(buffer, val)
    }

    /// Write actual values in the buffer-slot we got during `alloc()`
    pub fn set_value(&mut self, buffer: &mut Cursor<Vec<u8>>, val: T) -> Result<()> {
        // Save whereever the current cursor stands in the buffer
        let curr_pos = buffer.position();

        // Write the actual value we want at our position that
        // was determined by `alloc()` into the buffer
        buffer.set_position(self.position as u64);
        let bytes = unsafe {
            std::slice::from_raw_parts(&val as *const T as *const u8, std::mem::size_of::<T>())
        };
        let res = buffer.write_all(bytes);

        // Resetting whereever we were before updating this
        // regardless of the write-result
        buffer.set_position(curr_pos);

        res?;
        Ok(())
    }

    pub fn location(&self) -> MDLocationDescriptor {
        MDLocationDescriptor {
            data_size: std::mem::size_of::<T>() as u32,
            rva: self.position,
        }
    }
}

#[derive(Debug, PartialEq)]
pub struct MemoryArrayWriter<T: Default + Sized> {
    pub position: MDRVA,
    array_size: usize,
    phantom: std::marker::PhantomData<T>,
}

impl<T> MemoryArrayWriter<T>
where
    T: Default + Sized,
{
    /// Create a slot for a type T in the buffer, we can fill in the values in one go.
    pub fn alloc_from_array(buffer: &mut Cursor<Vec<u8>>, array: &[T]) -> Result<Self> {
        // Get position of this value (e.g. before we add ourselves there)
        let position = buffer.position();
        for val in array {
            let bytes = unsafe {
                std::slice::from_raw_parts(val as *const T as *const u8, std::mem::size_of::<T>())
            };
            buffer.write_all(bytes)?;
        }

        Ok(MemoryArrayWriter {
            position: position as u32,
            array_size: array.len(),
            phantom: std::marker::PhantomData::<T> {},
        })
    }

    /// Create a slot for a type T in the buffer, we can fill later with real values.
    /// This function fills it with `Default::default()`, which is less performant than
    /// using uninitialized memory, but safe.
    pub fn alloc_array(buffer: &mut Cursor<Vec<u8>>, array_size: usize) -> Result<Self> {
        // Get position of this value (e.g. before we add ourselves there)
        let position = buffer.position();
        for _ in 0..array_size {
            // Filling out the buffer with default-values
            let val: T = Default::default();
            let bytes = unsafe {
                std::slice::from_raw_parts(&val as *const T as *const u8, std::mem::size_of::<T>())
            };
            buffer.write_all(bytes)?;
        }

        Ok(MemoryArrayWriter {
            position: position as u32,
            array_size,
            phantom: std::marker::PhantomData::<T> {},
        })
    }

    /// Write actual values in the buffer-slot we got during `alloc()`
    pub fn set_value_at(
        &mut self,
        buffer: &mut Cursor<Vec<u8>>,
        val: T,
        index: usize,
    ) -> Result<()> {
        // Save whereever the current cursor stands in the buffer
        let curr_pos = buffer.position();

        // Write the actual value we want at our position that
        // was determined by `alloc()` into the buffer
        buffer.set_position(self.position as u64 + (std::mem::size_of::<T>() * index) as u64);
        let bytes = unsafe {
            std::slice::from_raw_parts(&val as *const T as *const u8, std::mem::size_of::<T>())
        };
        let res = buffer.write_all(bytes);

        // Resetting whereever we were before updating this
        // regardless of the write-result
        buffer.set_position(curr_pos);

        res?;
        Ok(())
    }

    pub fn location(&self) -> MDLocationDescriptor {
        MDLocationDescriptor {
            data_size: (self.array_size * std::mem::size_of::<T>()) as u32,
            rva: self.position,
        }
    }

    pub fn location_of_index(&self, idx: usize) -> MDLocationDescriptor {
        MDLocationDescriptor {
            data_size: std::mem::size_of::<T>() as u32,
            rva: self.position + (std::mem::size_of::<T>() * idx) as u32,
        }
    }
}

pub fn write_string_to_location(
    buffer: &mut Cursor<Vec<u8>>,
    text: &str,
) -> Result<MDLocationDescriptor> {
    let letters: Vec<u16> = text.encode_utf16().collect();

    // First write size of the string (x letters in u16, times the size of u16)
    let text_header = MemoryWriter::<u32>::alloc_with_val(
        buffer,
        (letters.len() * std::mem::size_of::<u16>()).try_into()?,
    )?;

    // Then write utf-16 letters after that
    let mut text_section = MemoryArrayWriter::<u16>::alloc_array(buffer, letters.len())?;
    for (index, letter) in letters.iter().enumerate() {
        text_section.set_value_at(buffer, *letter, index)?;
    }

    let mut location = text_header.location();
    location.data_size += text_section.location().data_size;

    Ok(location)
}
