/* Copyright 2018 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

use std::boxed::Box;
use std::convert::TryInto;
use std::str;
use std::vec::Vec;

use crate::limits::{
    MAX_WASM_FUNCTION_LOCALS, MAX_WASM_FUNCTION_PARAMS, MAX_WASM_FUNCTION_RETURNS,
    MAX_WASM_FUNCTION_SIZE, MAX_WASM_STRING_SIZE,
};

use crate::primitives::{
    BinaryReaderError, BrTable, CustomSectionKind, ExternalKind, FuncType, GlobalType, Ieee32,
    Ieee64, LinkingType, MemoryImmediate, MemoryType, NameType, Operator, RelocType,
    ResizableLimits, Result, SIMDLaneIndex, SectionCode, TableType, Type, TypeOrFuncType, V128,
};

const MAX_WASM_BR_TABLE_SIZE: usize = MAX_WASM_FUNCTION_SIZE;

fn is_name(name: &str, expected: &'static str) -> bool {
    name == expected
}

fn is_name_prefix(name: &str, prefix: &'static str) -> bool {
    name.starts_with(prefix)
}

const WASM_MAGIC_NUMBER: &[u8; 4] = b"\0asm";
const WASM_EXPERIMENTAL_VERSION: u32 = 0xd;
const WASM_SUPPORTED_VERSION: u32 = 0x1;

pub(crate) struct SectionHeader<'a> {
    pub code: SectionCode<'a>,
    pub payload_start: usize,
    pub payload_len: usize,
}

/// Bytecode range in the WebAssembly module.
#[derive(Copy, Clone, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct Range {
    /// The start bound of the range.
    pub start: usize,
    /// The end bound of the range.
    pub end: usize,
}

impl Range {
    /// Constructs a new instance of `Range`.
    ///
    /// # Panics
    /// If `start` is greater than `end`.
    pub fn new(start: usize, end: usize) -> Range {
        assert!(start <= end);
        Range { start, end }
    }

    /// Returns a new slice between `start` and `end - 1` from `data`.
    pub fn slice<'a>(&self, data: &'a [u8]) -> &'a [u8] {
        &data[self.start..self.end]
    }
}

/// A binary reader of the WebAssembly structures and types.
#[derive(Clone, Debug)]
pub struct BinaryReader<'a> {
    pub(crate) buffer: &'a [u8],
    pub(crate) position: usize,
    pub(crate) original_offset: usize,
}

impl<'a> BinaryReader<'a> {
    /// Constructs `BinaryReader` type.
    ///
    /// # Examples
    /// ```
    /// let fn_body = &vec![0x41, 0x00, 0x10, 0x00, 0x0B];
    /// let mut reader = wasmparser::BinaryReader::new(fn_body);
    /// while !reader.eof() {
    ///     let op = reader.read_operator();
    ///     println!("{:?}", op)
    /// }
    /// ```
    pub fn new(data: &[u8]) -> BinaryReader {
        BinaryReader {
            buffer: data,
            position: 0,
            original_offset: 0,
        }
    }

    /// Constructs a `BinaryReader` with an explicit starting offset.
    pub fn new_with_offset(data: &[u8], original_offset: usize) -> BinaryReader {
        BinaryReader {
            buffer: data,
            position: 0,
            original_offset,
        }
    }

    pub fn original_position(&self) -> usize {
        self.original_offset + self.position
    }

    /// Returns a range from the starting offset to the end of the buffer.
    pub fn range(&self) -> Range {
        Range {
            start: self.original_offset,
            end: self.original_offset + self.buffer.len(),
        }
    }

    fn ensure_has_byte(&self) -> Result<()> {
        if self.position < self.buffer.len() {
            Ok(())
        } else {
            Err(BinaryReaderError {
                message: "Unexpected EOF",
                offset: self.original_position(),
            })
        }
    }

    fn ensure_has_bytes(&self, len: usize) -> Result<()> {
        if self.position + len <= self.buffer.len() {
            Ok(())
        } else {
            Err(BinaryReaderError {
                message: "Unexpected EOF",
                offset: self.original_position(),
            })
        }
    }

    fn read_var_u1(&mut self) -> Result<u32> {
        let b = self.read_u8()?;
        if (b & 0xFE) != 0 {
            return Err(BinaryReaderError {
                message: "Invalid var_u1",
                offset: self.original_position() - 1,
            });
        }
        Ok(b)
    }

    fn read_var_i7(&mut self) -> Result<i32> {
        let b = self.read_u8()?;
        if (b & 0x80) != 0 {
            return Err(BinaryReaderError {
                message: "Invalid var_i7",
                offset: self.original_position() - 1,
            });
        }
        Ok((b << 25) as i32 >> 25)
    }

    pub(crate) fn read_var_u7(&mut self) -> Result<u32> {
        let b = self.read_u8()?;
        if (b & 0x80) != 0 {
            return Err(BinaryReaderError {
                message: "Invalid var_u7",
                offset: self.original_position() - 1,
            });
        }
        Ok(b)
    }

    pub fn read_type(&mut self) -> Result<Type> {
        let code = self.read_var_i7()?;
        match code {
            -0x01 => Ok(Type::I32),
            -0x02 => Ok(Type::I64),
            -0x03 => Ok(Type::F32),
            -0x04 => Ok(Type::F64),
            -0x05 => Ok(Type::V128),
            -0x10 => Ok(Type::AnyFunc),
            -0x11 => Ok(Type::AnyRef),
            -0x12 => Ok(Type::NullRef),
            -0x20 => Ok(Type::Func),
            -0x40 => Ok(Type::EmptyBlockType),
            _ => Err(BinaryReaderError {
                message: "Invalid type",
                offset: self.original_position() - 1,
            }),
        }
    }

    /// Read a `count` indicating the number of times to call `read_local_decl`.
    pub fn read_local_count(&mut self) -> Result<usize> {
        let local_count = self.read_var_u32()? as usize;
        if local_count > MAX_WASM_FUNCTION_LOCALS {
            return Err(BinaryReaderError {
                message: "local_count is out of bounds",
                offset: self.original_position() - 1,
            });
        }
        Ok(local_count)
    }

    /// Read a `(count, value_type)` declaration of local variables of the same type.
    pub fn read_local_decl(&mut self, locals_total: &mut usize) -> Result<(u32, Type)> {
        let count = self.read_var_u32()?;
        let value_type = self.read_type()?;
        *locals_total =
            locals_total
                .checked_add(count as usize)
                .ok_or_else(|| BinaryReaderError {
                    message: "locals_total is out of bounds",
                    offset: self.original_position() - 1,
                })?;
        if *locals_total > MAX_WASM_FUNCTION_LOCALS {
            return Err(BinaryReaderError {
                message: "locals_total is out of bounds",
                offset: self.original_position() - 1,
            });
        }
        Ok((count, value_type))
    }

    pub(crate) fn read_external_kind(&mut self) -> Result<ExternalKind> {
        let code = self.read_u8()?;
        match code {
            0 => Ok(ExternalKind::Function),
            1 => Ok(ExternalKind::Table),
            2 => Ok(ExternalKind::Memory),
            3 => Ok(ExternalKind::Global),
            _ => Err(BinaryReaderError {
                message: "Invalid external kind",
                offset: self.original_position() - 1,
            }),
        }
    }

    pub(crate) fn read_func_type(&mut self) -> Result<FuncType> {
        let form = self.read_type()?;
        let params_len = self.read_var_u32()? as usize;
        if params_len > MAX_WASM_FUNCTION_PARAMS {
            return Err(BinaryReaderError {
                message: "function params size is out of bound",
                offset: self.original_position() - 1,
            });
        }
        let mut params: Vec<Type> = Vec::with_capacity(params_len);
        for _ in 0..params_len {
            params.push(self.read_type()?);
        }
        let returns_len = self.read_var_u32()? as usize;
        if returns_len > MAX_WASM_FUNCTION_RETURNS {
            return Err(BinaryReaderError {
                message: "function returns size is out of bound",
                offset: self.original_position() - 1,
            });
        }
        let mut returns: Vec<Type> = Vec::with_capacity(returns_len);
        for _ in 0..returns_len {
            returns.push(self.read_type()?);
        }
        Ok(FuncType {
            form,
            params: params.into_boxed_slice(),
            returns: returns.into_boxed_slice(),
        })
    }

    fn read_resizable_limits(&mut self, max_present: bool) -> Result<ResizableLimits> {
        let initial = self.read_var_u32()?;
        let maximum = if max_present {
            Some(self.read_var_u32()?)
        } else {
            None
        };
        Ok(ResizableLimits { initial, maximum })
    }

    pub(crate) fn read_table_type(&mut self) -> Result<TableType> {
        let element_type = self.read_type()?;
        let flags = self.read_var_u32()?;
        if (flags & !0x1) != 0 {
            return Err(BinaryReaderError {
                message: "invalid table resizable limits flags",
                offset: self.original_position() - 1,
            });
        }
        let limits = self.read_resizable_limits((flags & 0x1) != 0)?;
        Ok(TableType {
            element_type,
            limits,
        })
    }

    pub(crate) fn read_memory_type(&mut self) -> Result<MemoryType> {
        let flags = self.read_var_u32()?;
        if (flags & !0x3) != 0 {
            return Err(BinaryReaderError {
                message: "invalid table resizable limits flags",
                offset: self.original_position() - 1,
            });
        }
        let limits = self.read_resizable_limits((flags & 0x1) != 0)?;
        let shared = (flags & 0x2) != 0;
        Ok(MemoryType { limits, shared })
    }

    pub(crate) fn read_global_type(&mut self) -> Result<GlobalType> {
        Ok(GlobalType {
            content_type: self.read_type()?,
            mutable: self.read_var_u1()? != 0,
        })
    }

    fn read_memarg(&mut self) -> Result<MemoryImmediate> {
        Ok(MemoryImmediate {
            flags: self.read_var_u32()?,
            offset: self.read_var_u32()?,
        })
    }

    pub(crate) fn read_section_code(&mut self, id: u32, offset: usize) -> Result<SectionCode<'a>> {
        match id {
            0 => {
                let name = self.read_string()?;
                let kind = if is_name(name, "name") {
                    CustomSectionKind::Name
                } else if is_name(name, "producers") {
                    CustomSectionKind::Producers
                } else if is_name(name, "sourceMappingURL") {
                    CustomSectionKind::SourceMappingURL
                } else if is_name_prefix(name, "reloc.") {
                    CustomSectionKind::Reloc
                } else if is_name(name, "linking") {
                    CustomSectionKind::Linking
                } else {
                    CustomSectionKind::Unknown
                };
                Ok(SectionCode::Custom { name, kind })
            }
            1 => Ok(SectionCode::Type),
            2 => Ok(SectionCode::Import),
            3 => Ok(SectionCode::Function),
            4 => Ok(SectionCode::Table),
            5 => Ok(SectionCode::Memory),
            6 => Ok(SectionCode::Global),
            7 => Ok(SectionCode::Export),
            8 => Ok(SectionCode::Start),
            9 => Ok(SectionCode::Element),
            10 => Ok(SectionCode::Code),
            11 => Ok(SectionCode::Data),
            12 => Ok(SectionCode::DataCount),
            _ => Err(BinaryReaderError {
                message: "Invalid section code",
                offset,
            }),
        }
    }

    fn read_br_table(&mut self) -> Result<BrTable<'a>> {
        let targets_len = self.read_var_u32()? as usize;
        if targets_len > MAX_WASM_BR_TABLE_SIZE {
            return Err(BinaryReaderError {
                message: "br_table size is out of bound",
                offset: self.original_position() - 1,
            });
        }
        let start = self.position;
        for _ in 0..targets_len {
            self.skip_var_32()?;
        }
        self.skip_var_32()?;
        Ok(BrTable {
            buffer: &self.buffer[start..self.position],
            cnt: targets_len as usize,
        })
    }

    /// Returns whether the `BinaryReader` has reached the end of the file.
    pub fn eof(&self) -> bool {
        self.position >= self.buffer.len()
    }

    /// Returns the `BinaryReader`'s current position.
    pub fn current_position(&self) -> usize {
        self.position
    }

    /// Returns the number of bytes remaining in the `BinaryReader`.
    pub fn bytes_remaining(&self) -> usize {
        self.buffer.len() - self.position
    }

    /// Advances the `BinaryReader` `size` bytes, and returns a slice from the
    /// current position of `size` length.
    ///
    /// # Errors
    /// If `size` exceeds the remaining length in `BinaryReader`.
    pub fn read_bytes(&mut self, size: usize) -> Result<&'a [u8]> {
        self.ensure_has_bytes(size)?;
        let start = self.position;
        self.position += size;
        Ok(&self.buffer[start..self.position])
    }

    /// Advances the `BinaryReader` four bytes and returns a `u32`.
    /// # Errors
    /// If `BinaryReader` has less than four bytes remaining.
    pub fn read_u32(&mut self) -> Result<u32> {
        self.ensure_has_bytes(4)?;
        let word = u32::from_le_bytes(
            self.buffer[self.position..self.position + 4]
                .try_into()
                .unwrap(),
        );
        self.position += 4;
        Ok(word)
    }

    /// Advances the `BinaryReader` eight bytes and returns a `u64`.
    /// # Errors
    /// If `BinaryReader` has less than eight bytes remaining.
    pub fn read_u64(&mut self) -> Result<u64> {
        self.ensure_has_bytes(8)?;
        let word = u64::from_le_bytes(
            self.buffer[self.position..self.position + 8]
                .try_into()
                .unwrap(),
        );
        self.position += 8;
        Ok(word)
    }

    /// Advances the `BinaryReader` a single byte, and returns the data as
    /// a `u32`.
    /// # Errors
    /// If `BinaryReader` has no bytes remaining.
    pub fn read_u8(&mut self) -> Result<u32> {
        self.ensure_has_byte()?;
        let b = u32::from(self.buffer[self.position]);
        self.position += 1;
        Ok(b)
    }

    /// Advances the `BinaryReader` up to two bytes to parse a variable
    /// length integer as a `u8`.
    /// # Errors
    /// If `BinaryReader` has less than one or two bytes remaining, or the
    /// integer is larger than eight bits.
    pub fn read_var_u8(&mut self) -> Result<u32> {
        // Optimization for single byte i32.
        let byte = self.read_u8()?;
        if (byte & 0x80) == 0 {
            return Ok(byte);
        }

        let result = (self.read_u8()? << 7) | (byte & 0x7F);
        if result >= 0x100 {
            return Err(BinaryReaderError {
                message: "Invalid var_u8",
                offset: self.original_position() - 1,
            });
        }
        Ok(result)
    }

    /// Advances the `BinaryReader` up to four bytes to parse a variable
    /// length integer as a `u32`.
    /// # Errors
    /// If `BinaryReader` has less than one or up to four bytes remaining, or
    /// the integer is larger than 32 bits.
    pub fn read_var_u32(&mut self) -> Result<u32> {
        // Optimization for single byte i32.
        let byte = self.read_u8()?;
        if (byte & 0x80) == 0 {
            return Ok(byte);
        }

        let mut result = byte & 0x7F;
        let mut shift = 7;
        loop {
            let byte = self.read_u8()?;
            result |= ((byte & 0x7F) as u32) << shift;
            if shift >= 25 && (byte >> (32 - shift)) != 0 {
                // The continuation bit or unused bits are set.
                return Err(BinaryReaderError {
                    message: "Invalid var_u32",
                    offset: self.original_position() - 1,
                });
            }
            shift += 7;
            if (byte & 0x80) == 0 {
                break;
            }
        }
        Ok(result)
    }

    /// Advances the `BinaryReader` up to four bytes over a variable length 32
    /// bit integer, discarding the result.
    /// # Errors
    /// If `BinaryReader` has less than one or up to four bytes remaining, or
    /// the integer is larger than 32 bits.
    pub fn skip_var_32(&mut self) -> Result<()> {
        for _ in 0..5 {
            let byte = self.read_u8()?;
            if (byte & 0x80) == 0 {
                return Ok(());
            }
        }
        Err(BinaryReaderError {
            message: "Invalid var_32",
            offset: self.original_position() - 1,
        })
    }

    /// Alias method for `BinaryReader::skip_var_u32`.
    pub fn skip_type(&mut self) -> Result<()> {
        self.skip_var_32()
    }

    /// Advances the `BinaryReader` `len` bytes, skipping the result.
    /// # Errors
    /// If `BinaryReader` has less than `len` bytes remaining.
    pub fn skip_bytes(&mut self, len: usize) -> Result<()> {
        self.ensure_has_bytes(len)?;
        self.position += len;
        Ok(())
    }

    /// Advances the `BinaryReader` past a WebAssembly string. This method does
    /// not perform any utf-8 validation.
    /// # Errors
    /// If `BinaryReader` has less than four bytes, the string's length exceeds
    /// the remaining bytes, or the string length
    /// exceeds `limits::MAX_WASM_STRING_SIZE`.
    pub fn skip_string(&mut self) -> Result<()> {
        let len = self.read_var_u32()? as usize;
        if len > MAX_WASM_STRING_SIZE {
            return Err(BinaryReaderError {
                message: "string size in out of bounds",
                offset: self.original_position() - 1,
            });
        }
        self.skip_bytes(len)
    }

    pub(crate) fn skip_to(&mut self, position: usize) {
        assert!(
            self.position <= position && position <= self.buffer.len(),
            "skip_to allowed only into region past current position"
        );
        self.position = position;
    }

    /// Advances the `BinaryReader` up to four bytes to parse a variable
    /// length integer as a `i32`.
    /// # Errors
    /// If `BinaryReader` has less than one or up to four bytes remaining, or
    /// the integer is larger than 32 bits.
    pub fn read_var_i32(&mut self) -> Result<i32> {
        // Optimization for single byte i32.
        let byte = self.read_u8()?;
        if (byte & 0x80) == 0 {
            return Ok(((byte as i32) << 25) >> 25);
        }

        let mut result = (byte & 0x7F) as i32;
        let mut shift = 7;
        loop {
            let byte = self.read_u8()?;
            result |= ((byte & 0x7F) as i32) << shift;
            if shift >= 25 {
                let continuation_bit = (byte & 0x80) != 0;
                let sign_and_unused_bit = (byte << 1) as i8 >> (32 - shift);
                if continuation_bit || (sign_and_unused_bit != 0 && sign_and_unused_bit != -1) {
                    return Err(BinaryReaderError {
                        message: "Invalid var_i32",
                        offset: self.original_position() - 1,
                    });
                }
                return Ok(result);
            }
            shift += 7;
            if (byte & 0x80) == 0 {
                break;
            }
        }
        let ashift = 32 - shift;
        Ok((result << ashift) >> ashift)
    }

    /// Advances the `BinaryReader` up to four bytes to parse a variable
    /// length integer as a signed 33 bit integer, returned as a `i64`.
    /// # Errors
    /// If `BinaryReader` has less than one or up to five bytes remaining, or
    /// the integer is larger than 33 bits.
    pub fn read_var_s33(&mut self) -> Result<i64> {
        // Optimization for single byte.
        let byte = self.read_u8()?;
        if (byte & 0x80) == 0 {
            return Ok(((byte as i8) << 1) as i64 >> 1);
        }

        let mut result = (byte & 0x7F) as i64;
        let mut shift = 7;
        loop {
            let byte = self.read_u8()?;
            result |= ((byte & 0x7F) as i64) << shift;
            if shift >= 25 {
                let continuation_bit = (byte & 0x80) != 0;
                let sign_and_unused_bit = (byte << 1) as i8 >> (33 - shift);
                if continuation_bit || (sign_and_unused_bit != 0 && sign_and_unused_bit != -1) {
                    return Err(BinaryReaderError {
                        message: "Invalid var_s33",
                        offset: self.original_position() - 1,
                    });
                }
                return Ok(result);
            }
            shift += 7;
            if (byte & 0x80) == 0 {
                break;
            }
        }
        let ashift = 64 - shift;
        Ok((result << ashift) >> ashift)
    }

    /// Advances the `BinaryReader` up to eight bytes to parse a variable
    /// length integer as a 64 bit integer, returned as a `i64`.
    /// # Errors
    /// If `BinaryReader` has less than one or up to eight bytes remaining, or
    /// the integer is larger than 64 bits.
    pub fn read_var_i64(&mut self) -> Result<i64> {
        let mut result: i64 = 0;
        let mut shift = 0;
        loop {
            let byte = self.read_u8()?;
            result |= i64::from(byte & 0x7F) << shift;
            if shift >= 57 {
                let continuation_bit = (byte & 0x80) != 0;
                let sign_and_unused_bit = ((byte << 1) as i8) >> (64 - shift);
                if continuation_bit || (sign_and_unused_bit != 0 && sign_and_unused_bit != -1) {
                    return Err(BinaryReaderError {
                        message: "Invalid var_i64",
                        offset: self.original_position() - 1,
                    });
                }
                return Ok(result);
            }
            shift += 7;
            if (byte & 0x80) == 0 {
                break;
            }
        }
        let ashift = 64 - shift;
        Ok((result << ashift) >> ashift)
    }

    /// Advances the `BinaryReader` up to four bytes to parse a variable
    /// length integer as a 32 bit floating point integer, returned as `Ieee32`.
    /// # Errors
    /// If `BinaryReader` has less than one or up to four bytes remaining, or
    /// the integer is larger than 32 bits.
    pub fn read_f32(&mut self) -> Result<Ieee32> {
        let value = self.read_u32()?;
        Ok(Ieee32(value))
    }

    /// Advances the `BinaryReader` up to four bytes to parse a variable
    /// length integer as a 32 bit floating point integer, returned as `Ieee32`.
    /// # Errors
    /// If `BinaryReader` has less than one or up to four bytes remaining, or
    /// the integer is larger than 32 bits.
    pub fn read_f64(&mut self) -> Result<Ieee64> {
        let value = self.read_u64()?;
        Ok(Ieee64(value))
    }

    /// Reads a WebAssembly string from the module.
    /// # Errors
    /// If `BinaryReader` has less than up to four bytes remaining, the string's
    /// length exceeds the remaining bytes, the string's length exceeds
    /// `limits::MAX_WASM_STRING_SIZE`, or the string contains invalid utf-8.
    pub fn read_string(&mut self) -> Result<&'a str> {
        let len = self.read_var_u32()? as usize;
        if len > MAX_WASM_STRING_SIZE {
            return Err(BinaryReaderError {
                message: "string size in out of bounds",
                offset: self.original_position() - 1,
            });
        }
        let bytes = self.read_bytes(len)?;
        str::from_utf8(bytes).map_err(|_| BinaryReaderError {
            message: "non-utf8 string",
            offset: self.original_position() - 1,
        })
    }

    fn read_memarg_of_align(&mut self, max_align: u32) -> Result<MemoryImmediate> {
        let imm = self.read_memarg()?;
        if imm.flags > max_align {
            return Err(BinaryReaderError {
                message: "Unexpected memarg alignment",
                offset: self.original_position() - 1,
            });
        }
        Ok(imm)
    }

    fn read_0xfe_operator(&mut self) -> Result<Operator<'a>> {
        let code = self.read_u8()? as u8;
        Ok(match code {
            0x00 => Operator::AtomicNotify {
                memarg: self.read_memarg_of_align(2)?,
            },
            0x01 => Operator::I32AtomicWait {
                memarg: self.read_memarg_of_align(2)?,
            },
            0x02 => Operator::I64AtomicWait {
                memarg: self.read_memarg_of_align(3)?,
            },
            0x03 => Operator::AtomicFence {
                flags: self.read_u8()? as u8,
            },
            0x10 => Operator::I32AtomicLoad {
                memarg: self.read_memarg_of_align(2)?,
            },
            0x11 => Operator::I64AtomicLoad {
                memarg: self.read_memarg_of_align(3)?,
            },
            0x12 => Operator::I32AtomicLoad8U {
                memarg: self.read_memarg_of_align(0)?,
            },
            0x13 => Operator::I32AtomicLoad16U {
                memarg: self.read_memarg_of_align(1)?,
            },
            0x14 => Operator::I64AtomicLoad8U {
                memarg: self.read_memarg_of_align(0)?,
            },
            0x15 => Operator::I64AtomicLoad16U {
                memarg: self.read_memarg_of_align(1)?,
            },
            0x16 => Operator::I64AtomicLoad32U {
                memarg: self.read_memarg_of_align(2)?,
            },
            0x17 => Operator::I32AtomicStore {
                memarg: self.read_memarg_of_align(2)?,
            },
            0x18 => Operator::I64AtomicStore {
                memarg: self.read_memarg_of_align(3)?,
            },
            0x19 => Operator::I32AtomicStore8 {
                memarg: self.read_memarg_of_align(0)?,
            },
            0x1a => Operator::I32AtomicStore16 {
                memarg: self.read_memarg_of_align(1)?,
            },
            0x1b => Operator::I64AtomicStore8 {
                memarg: self.read_memarg_of_align(0)?,
            },
            0x1c => Operator::I64AtomicStore16 {
                memarg: self.read_memarg_of_align(1)?,
            },
            0x1d => Operator::I64AtomicStore32 {
                memarg: self.read_memarg_of_align(2)?,
            },
            0x1e => Operator::I32AtomicRmwAdd {
                memarg: self.read_memarg_of_align(2)?,
            },
            0x1f => Operator::I64AtomicRmwAdd {
                memarg: self.read_memarg_of_align(3)?,
            },
            0x20 => Operator::I32AtomicRmw8AddU {
                memarg: self.read_memarg_of_align(0)?,
            },
            0x21 => Operator::I32AtomicRmw16AddU {
                memarg: self.read_memarg_of_align(1)?,
            },
            0x22 => Operator::I64AtomicRmw8AddU {
                memarg: self.read_memarg_of_align(0)?,
            },
            0x23 => Operator::I64AtomicRmw16AddU {
                memarg: self.read_memarg_of_align(1)?,
            },
            0x24 => Operator::I64AtomicRmw32AddU {
                memarg: self.read_memarg_of_align(2)?,
            },
            0x25 => Operator::I32AtomicRmwSub {
                memarg: self.read_memarg_of_align(2)?,
            },
            0x26 => Operator::I64AtomicRmwSub {
                memarg: self.read_memarg_of_align(3)?,
            },
            0x27 => Operator::I32AtomicRmw8SubU {
                memarg: self.read_memarg_of_align(0)?,
            },
            0x28 => Operator::I32AtomicRmw16SubU {
                memarg: self.read_memarg_of_align(1)?,
            },
            0x29 => Operator::I64AtomicRmw8SubU {
                memarg: self.read_memarg_of_align(0)?,
            },
            0x2a => Operator::I64AtomicRmw16SubU {
                memarg: self.read_memarg_of_align(1)?,
            },
            0x2b => Operator::I64AtomicRmw32SubU {
                memarg: self.read_memarg_of_align(2)?,
            },
            0x2c => Operator::I32AtomicRmwAnd {
                memarg: self.read_memarg_of_align(2)?,
            },
            0x2d => Operator::I64AtomicRmwAnd {
                memarg: self.read_memarg_of_align(3)?,
            },
            0x2e => Operator::I32AtomicRmw8AndU {
                memarg: self.read_memarg_of_align(0)?,
            },
            0x2f => Operator::I32AtomicRmw16AndU {
                memarg: self.read_memarg_of_align(1)?,
            },
            0x30 => Operator::I64AtomicRmw8AndU {
                memarg: self.read_memarg_of_align(0)?,
            },
            0x31 => Operator::I64AtomicRmw16AndU {
                memarg: self.read_memarg_of_align(1)?,
            },
            0x32 => Operator::I64AtomicRmw32AndU {
                memarg: self.read_memarg_of_align(2)?,
            },
            0x33 => Operator::I32AtomicRmwOr {
                memarg: self.read_memarg_of_align(2)?,
            },
            0x34 => Operator::I64AtomicRmwOr {
                memarg: self.read_memarg_of_align(3)?,
            },
            0x35 => Operator::I32AtomicRmw8OrU {
                memarg: self.read_memarg_of_align(0)?,
            },
            0x36 => Operator::I32AtomicRmw16OrU {
                memarg: self.read_memarg_of_align(1)?,
            },
            0x37 => Operator::I64AtomicRmw8OrU {
                memarg: self.read_memarg_of_align(0)?,
            },
            0x38 => Operator::I64AtomicRmw16OrU {
                memarg: self.read_memarg_of_align(1)?,
            },
            0x39 => Operator::I64AtomicRmw32OrU {
                memarg: self.read_memarg_of_align(2)?,
            },
            0x3a => Operator::I32AtomicRmwXor {
                memarg: self.read_memarg_of_align(2)?,
            },
            0x3b => Operator::I64AtomicRmwXor {
                memarg: self.read_memarg_of_align(3)?,
            },
            0x3c => Operator::I32AtomicRmw8XorU {
                memarg: self.read_memarg_of_align(0)?,
            },
            0x3d => Operator::I32AtomicRmw16XorU {
                memarg: self.read_memarg_of_align(1)?,
            },
            0x3e => Operator::I64AtomicRmw8XorU {
                memarg: self.read_memarg_of_align(0)?,
            },
            0x3f => Operator::I64AtomicRmw16XorU {
                memarg: self.read_memarg_of_align(1)?,
            },
            0x40 => Operator::I64AtomicRmw32XorU {
                memarg: self.read_memarg_of_align(2)?,
            },
            0x41 => Operator::I32AtomicRmwXchg {
                memarg: self.read_memarg_of_align(2)?,
            },
            0x42 => Operator::I64AtomicRmwXchg {
                memarg: self.read_memarg_of_align(3)?,
            },
            0x43 => Operator::I32AtomicRmw8XchgU {
                memarg: self.read_memarg_of_align(0)?,
            },
            0x44 => Operator::I32AtomicRmw16XchgU {
                memarg: self.read_memarg_of_align(1)?,
            },
            0x45 => Operator::I64AtomicRmw8XchgU {
                memarg: self.read_memarg_of_align(0)?,
            },
            0x46 => Operator::I64AtomicRmw16XchgU {
                memarg: self.read_memarg_of_align(1)?,
            },
            0x47 => Operator::I64AtomicRmw32XchgU {
                memarg: self.read_memarg_of_align(2)?,
            },
            0x48 => Operator::I32AtomicRmwCmpxchg {
                memarg: self.read_memarg_of_align(2)?,
            },
            0x49 => Operator::I64AtomicRmwCmpxchg {
                memarg: self.read_memarg_of_align(3)?,
            },
            0x4a => Operator::I32AtomicRmw8CmpxchgU {
                memarg: self.read_memarg_of_align(0)?,
            },
            0x4b => Operator::I32AtomicRmw16CmpxchgU {
                memarg: self.read_memarg_of_align(1)?,
            },
            0x4c => Operator::I64AtomicRmw8CmpxchgU {
                memarg: self.read_memarg_of_align(0)?,
            },
            0x4d => Operator::I64AtomicRmw16CmpxchgU {
                memarg: self.read_memarg_of_align(1)?,
            },
            0x4e => Operator::I64AtomicRmw32CmpxchgU {
                memarg: self.read_memarg_of_align(2)?,
            },

            _ => {
                return Err(BinaryReaderError {
                    message: "Unknown 0xFE opcode",
                    offset: self.original_position() - 1,
                });
            }
        })
    }

    fn read_blocktype(&mut self) -> Result<TypeOrFuncType> {
        let position = self.position;
        if let Ok(ty) = self.read_type() {
            Ok(TypeOrFuncType::Type(ty))
        } else {
            self.position = position;
            let idx = self.read_var_s33()?;
            if idx < 0 || idx > (std::u32::MAX as i64) {
                return Err(BinaryReaderError {
                    message: "invalid function type",
                    offset: position,
                });
            }
            Ok(TypeOrFuncType::FuncType(idx as u32))
        }
    }

    /// Reads the next available `Operator`.
    /// # Errors
    /// If `BinaryReader` has less bytes remaining than required to parse
    /// the `Operator`.
    pub fn read_operator(&mut self) -> Result<Operator<'a>> {
        let code = self.read_u8()? as u8;
        Ok(match code {
            0x00 => Operator::Unreachable,
            0x01 => Operator::Nop,
            0x02 => Operator::Block {
                ty: self.read_blocktype()?,
            },
            0x03 => Operator::Loop {
                ty: self.read_blocktype()?,
            },
            0x04 => Operator::If {
                ty: self.read_blocktype()?,
            },
            0x05 => Operator::Else,
            0x0b => Operator::End,
            0x0c => Operator::Br {
                relative_depth: self.read_var_u32()?,
            },
            0x0d => Operator::BrIf {
                relative_depth: self.read_var_u32()?,
            },
            0x0e => Operator::BrTable {
                table: self.read_br_table()?,
            },
            0x0f => Operator::Return,
            0x10 => Operator::Call {
                function_index: self.read_var_u32()?,
            },
            0x11 => Operator::CallIndirect {
                index: self.read_var_u32()?,
                table_index: self.read_var_u32()?,
            },
            0x1a => Operator::Drop,
            0x1b => Operator::Select,
            0x1c => {
                let results = self.read_var_u32()?;
                if results != 1 {
                    return Err(BinaryReaderError {
                        message: "bad number of results",
                        offset: self.position,
                    });
                }
                Operator::TypedSelect {
                    ty: self.read_type()?,
                }
            }
            0x20 => Operator::LocalGet {
                local_index: self.read_var_u32()?,
            },
            0x21 => Operator::LocalSet {
                local_index: self.read_var_u32()?,
            },
            0x22 => Operator::LocalTee {
                local_index: self.read_var_u32()?,
            },
            0x23 => Operator::GlobalGet {
                global_index: self.read_var_u32()?,
            },
            0x24 => Operator::GlobalSet {
                global_index: self.read_var_u32()?,
            },
            0x25 => Operator::TableGet {
                table: self.read_var_u32()?,
            },
            0x26 => Operator::TableSet {
                table: self.read_var_u32()?,
            },
            0x28 => Operator::I32Load {
                memarg: self.read_memarg()?,
            },
            0x29 => Operator::I64Load {
                memarg: self.read_memarg()?,
            },
            0x2a => Operator::F32Load {
                memarg: self.read_memarg()?,
            },
            0x2b => Operator::F64Load {
                memarg: self.read_memarg()?,
            },
            0x2c => Operator::I32Load8S {
                memarg: self.read_memarg()?,
            },
            0x2d => Operator::I32Load8U {
                memarg: self.read_memarg()?,
            },
            0x2e => Operator::I32Load16S {
                memarg: self.read_memarg()?,
            },
            0x2f => Operator::I32Load16U {
                memarg: self.read_memarg()?,
            },
            0x30 => Operator::I64Load8S {
                memarg: self.read_memarg()?,
            },
            0x31 => Operator::I64Load8U {
                memarg: self.read_memarg()?,
            },
            0x32 => Operator::I64Load16S {
                memarg: self.read_memarg()?,
            },
            0x33 => Operator::I64Load16U {
                memarg: self.read_memarg()?,
            },
            0x34 => Operator::I64Load32S {
                memarg: self.read_memarg()?,
            },
            0x35 => Operator::I64Load32U {
                memarg: self.read_memarg()?,
            },
            0x36 => Operator::I32Store {
                memarg: self.read_memarg()?,
            },
            0x37 => Operator::I64Store {
                memarg: self.read_memarg()?,
            },
            0x38 => Operator::F32Store {
                memarg: self.read_memarg()?,
            },
            0x39 => Operator::F64Store {
                memarg: self.read_memarg()?,
            },
            0x3a => Operator::I32Store8 {
                memarg: self.read_memarg()?,
            },
            0x3b => Operator::I32Store16 {
                memarg: self.read_memarg()?,
            },
            0x3c => Operator::I64Store8 {
                memarg: self.read_memarg()?,
            },
            0x3d => Operator::I64Store16 {
                memarg: self.read_memarg()?,
            },
            0x3e => Operator::I64Store32 {
                memarg: self.read_memarg()?,
            },
            0x3f => Operator::MemorySize {
                reserved: self.read_var_u1()?,
            },
            0x40 => Operator::MemoryGrow {
                reserved: self.read_var_u1()?,
            },
            0x41 => Operator::I32Const {
                value: self.read_var_i32()?,
            },
            0x42 => Operator::I64Const {
                value: self.read_var_i64()?,
            },
            0x43 => Operator::F32Const {
                value: self.read_f32()?,
            },
            0x44 => Operator::F64Const {
                value: self.read_f64()?,
            },
            0x45 => Operator::I32Eqz,
            0x46 => Operator::I32Eq,
            0x47 => Operator::I32Ne,
            0x48 => Operator::I32LtS,
            0x49 => Operator::I32LtU,
            0x4a => Operator::I32GtS,
            0x4b => Operator::I32GtU,
            0x4c => Operator::I32LeS,
            0x4d => Operator::I32LeU,
            0x4e => Operator::I32GeS,
            0x4f => Operator::I32GeU,
            0x50 => Operator::I64Eqz,
            0x51 => Operator::I64Eq,
            0x52 => Operator::I64Ne,
            0x53 => Operator::I64LtS,
            0x54 => Operator::I64LtU,
            0x55 => Operator::I64GtS,
            0x56 => Operator::I64GtU,
            0x57 => Operator::I64LeS,
            0x58 => Operator::I64LeU,
            0x59 => Operator::I64GeS,
            0x5a => Operator::I64GeU,
            0x5b => Operator::F32Eq,
            0x5c => Operator::F32Ne,
            0x5d => Operator::F32Lt,
            0x5e => Operator::F32Gt,
            0x5f => Operator::F32Le,
            0x60 => Operator::F32Ge,
            0x61 => Operator::F64Eq,
            0x62 => Operator::F64Ne,
            0x63 => Operator::F64Lt,
            0x64 => Operator::F64Gt,
            0x65 => Operator::F64Le,
            0x66 => Operator::F64Ge,
            0x67 => Operator::I32Clz,
            0x68 => Operator::I32Ctz,
            0x69 => Operator::I32Popcnt,
            0x6a => Operator::I32Add,
            0x6b => Operator::I32Sub,
            0x6c => Operator::I32Mul,
            0x6d => Operator::I32DivS,
            0x6e => Operator::I32DivU,
            0x6f => Operator::I32RemS,
            0x70 => Operator::I32RemU,
            0x71 => Operator::I32And,
            0x72 => Operator::I32Or,
            0x73 => Operator::I32Xor,
            0x74 => Operator::I32Shl,
            0x75 => Operator::I32ShrS,
            0x76 => Operator::I32ShrU,
            0x77 => Operator::I32Rotl,
            0x78 => Operator::I32Rotr,
            0x79 => Operator::I64Clz,
            0x7a => Operator::I64Ctz,
            0x7b => Operator::I64Popcnt,
            0x7c => Operator::I64Add,
            0x7d => Operator::I64Sub,
            0x7e => Operator::I64Mul,
            0x7f => Operator::I64DivS,
            0x80 => Operator::I64DivU,
            0x81 => Operator::I64RemS,
            0x82 => Operator::I64RemU,
            0x83 => Operator::I64And,
            0x84 => Operator::I64Or,
            0x85 => Operator::I64Xor,
            0x86 => Operator::I64Shl,
            0x87 => Operator::I64ShrS,
            0x88 => Operator::I64ShrU,
            0x89 => Operator::I64Rotl,
            0x8a => Operator::I64Rotr,
            0x8b => Operator::F32Abs,
            0x8c => Operator::F32Neg,
            0x8d => Operator::F32Ceil,
            0x8e => Operator::F32Floor,
            0x8f => Operator::F32Trunc,
            0x90 => Operator::F32Nearest,
            0x91 => Operator::F32Sqrt,
            0x92 => Operator::F32Add,
            0x93 => Operator::F32Sub,
            0x94 => Operator::F32Mul,
            0x95 => Operator::F32Div,
            0x96 => Operator::F32Min,
            0x97 => Operator::F32Max,
            0x98 => Operator::F32Copysign,
            0x99 => Operator::F64Abs,
            0x9a => Operator::F64Neg,
            0x9b => Operator::F64Ceil,
            0x9c => Operator::F64Floor,
            0x9d => Operator::F64Trunc,
            0x9e => Operator::F64Nearest,
            0x9f => Operator::F64Sqrt,
            0xa0 => Operator::F64Add,
            0xa1 => Operator::F64Sub,
            0xa2 => Operator::F64Mul,
            0xa3 => Operator::F64Div,
            0xa4 => Operator::F64Min,
            0xa5 => Operator::F64Max,
            0xa6 => Operator::F64Copysign,
            0xa7 => Operator::I32WrapI64,
            0xa8 => Operator::I32TruncF32S,
            0xa9 => Operator::I32TruncF32U,
            0xaa => Operator::I32TruncF64S,
            0xab => Operator::I32TruncF64U,
            0xac => Operator::I64ExtendI32S,
            0xad => Operator::I64ExtendI32U,
            0xae => Operator::I64TruncF32S,
            0xaf => Operator::I64TruncF32U,
            0xb0 => Operator::I64TruncF64S,
            0xb1 => Operator::I64TruncF64U,
            0xb2 => Operator::F32ConvertI32S,
            0xb3 => Operator::F32ConvertI32U,
            0xb4 => Operator::F32ConvertI64S,
            0xb5 => Operator::F32ConvertI64U,
            0xb6 => Operator::F32DemoteF64,
            0xb7 => Operator::F64ConvertI32S,
            0xb8 => Operator::F64ConvertI32U,
            0xb9 => Operator::F64ConvertI64S,
            0xba => Operator::F64ConvertI64U,
            0xbb => Operator::F64PromoteF32,
            0xbc => Operator::I32ReinterpretF32,
            0xbd => Operator::I64ReinterpretF64,
            0xbe => Operator::F32ReinterpretI32,
            0xbf => Operator::F64ReinterpretI64,

            0xc0 => Operator::I32Extend8S,
            0xc1 => Operator::I32Extend16S,
            0xc2 => Operator::I64Extend8S,
            0xc3 => Operator::I64Extend16S,
            0xc4 => Operator::I64Extend32S,

            0xd0 => Operator::RefNull,
            0xd1 => Operator::RefIsNull,
            0xd2 => Operator::RefFunc {
                function_index: self.read_var_u32()?,
            },

            0xfc => self.read_0xfc_operator()?,
            0xfd => self.read_0xfd_operator()?,
            0xfe => self.read_0xfe_operator()?,

            _ => {
                return Err(BinaryReaderError {
                    message: "Unknown opcode",
                    offset: self.original_position() - 1,
                });
            }
        })
    }

    fn read_0xfc_operator(&mut self) -> Result<Operator<'a>> {
        let code = self.read_u8()? as u8;
        Ok(match code {
            0x00 => Operator::I32TruncSatF32S,
            0x01 => Operator::I32TruncSatF32U,
            0x02 => Operator::I32TruncSatF64S,
            0x03 => Operator::I32TruncSatF64U,
            0x04 => Operator::I64TruncSatF32S,
            0x05 => Operator::I64TruncSatF32U,
            0x06 => Operator::I64TruncSatF64S,
            0x07 => Operator::I64TruncSatF64U,

            0x08 => {
                let segment = self.read_var_u32()?;
                let mem = self.read_u8()?;
                if mem != 0 {
                    return Err(BinaryReaderError {
                        message: "reserved byte must be zero",
                        offset: self.original_position() - 1,
                    });
                }
                Operator::MemoryInit { segment }
            }
            0x09 => {
                let segment = self.read_var_u32()?;
                Operator::DataDrop { segment }
            }
            0x0a => {
                let dst = self.read_u8()?;
                if dst != 0 {
                    return Err(BinaryReaderError {
                        message: "reserved byte must be zero",
                        offset: self.original_position() - 1,
                    });
                }
                let src = self.read_u8()?;
                if src != 0 {
                    return Err(BinaryReaderError {
                        message: "reserved byte must be zero",
                        offset: self.original_position() - 1,
                    });
                }
                Operator::MemoryCopy
            }
            0x0b => {
                let mem = self.read_u8()?;
                if mem != 0 {
                    return Err(BinaryReaderError {
                        message: "reserved byte must be zero",
                        offset: self.original_position() - 1,
                    });
                }
                Operator::MemoryFill
            }
            0x0c => {
                let segment = self.read_var_u32()?;
                let table = self.read_var_u32()?;
                Operator::TableInit { segment, table }
            }
            0x0d => {
                let segment = self.read_var_u32()?;
                Operator::ElemDrop { segment }
            }
            0x0e => {
                let dst_table = self.read_var_u32()?;
                let src_table = self.read_var_u32()?;
                Operator::TableCopy {
                    src_table,
                    dst_table,
                }
            }

            0x0f => {
                let table = self.read_var_u32()?;
                Operator::TableGrow { table }
            }
            0x10 => {
                let table = self.read_var_u32()?;
                Operator::TableSize { table }
            }

            0x11 => {
                let table = self.read_var_u32()?;
                Operator::TableFill { table }
            }

            _ => {
                return Err(BinaryReaderError {
                    message: "Unknown 0xfc opcode",
                    offset: self.original_position() - 1,
                });
            }
        })
    }

    fn read_lane_index(&mut self, max: u32) -> Result<SIMDLaneIndex> {
        let index = self.read_u8()?;
        if index >= max {
            return Err(BinaryReaderError {
                message: "invalid lane index",
                offset: self.original_position() - 1,
            });
        }
        Ok(index as SIMDLaneIndex)
    }

    fn read_v128(&mut self) -> Result<V128> {
        let mut bytes = [0; 16];
        bytes.clone_from_slice(self.read_bytes(16)?);
        Ok(V128(bytes))
    }

    fn read_0xfd_operator(&mut self) -> Result<Operator<'a>> {
        let code = self.read_var_u8()? as u8;
        Ok(match code {
            0x00 => Operator::V128Load {
                memarg: self.read_memarg()?,
            },
            0x01 => Operator::V128Store {
                memarg: self.read_memarg()?,
            },
            0x02 => Operator::V128Const {
                value: self.read_v128()?,
            },
            0x04 => Operator::I8x16Splat,
            0x05 => Operator::I8x16ExtractLaneS {
                lane: self.read_lane_index(16)?,
            },
            0x06 => Operator::I8x16ExtractLaneU {
                lane: self.read_lane_index(16)?,
            },
            0x07 => Operator::I8x16ReplaceLane {
                lane: self.read_lane_index(16)?,
            },
            0x08 => Operator::I16x8Splat,
            0x09 => Operator::I16x8ExtractLaneS {
                lane: self.read_lane_index(8)?,
            },
            0x0a => Operator::I16x8ExtractLaneU {
                lane: self.read_lane_index(8)?,
            },
            0x0b => Operator::I16x8ReplaceLane {
                lane: self.read_lane_index(8)?,
            },
            0x0c => Operator::I32x4Splat,
            0x0d => Operator::I32x4ExtractLane {
                lane: self.read_lane_index(4)?,
            },
            0x0e => Operator::I32x4ReplaceLane {
                lane: self.read_lane_index(4)?,
            },
            0x0f => Operator::I64x2Splat,
            0x10 => Operator::I64x2ExtractLane {
                lane: self.read_lane_index(2)?,
            },
            0x11 => Operator::I64x2ReplaceLane {
                lane: self.read_lane_index(2)?,
            },
            0x12 => Operator::F32x4Splat,
            0x13 => Operator::F32x4ExtractLane {
                lane: self.read_lane_index(4)?,
            },
            0x14 => Operator::F32x4ReplaceLane {
                lane: self.read_lane_index(4)?,
            },
            0x15 => Operator::F64x2Splat,
            0x16 => Operator::F64x2ExtractLane {
                lane: self.read_lane_index(2)?,
            },
            0x17 => Operator::F64x2ReplaceLane {
                lane: self.read_lane_index(2)?,
            },
            0x18 => Operator::I8x16Eq,
            0x19 => Operator::I8x16Ne,
            0x1a => Operator::I8x16LtS,
            0x1b => Operator::I8x16LtU,
            0x1c => Operator::I8x16GtS,
            0x1d => Operator::I8x16GtU,
            0x1e => Operator::I8x16LeS,
            0x1f => Operator::I8x16LeU,
            0x20 => Operator::I8x16GeS,
            0x21 => Operator::I8x16GeU,
            0x22 => Operator::I16x8Eq,
            0x23 => Operator::I16x8Ne,
            0x24 => Operator::I16x8LtS,
            0x25 => Operator::I16x8LtU,
            0x26 => Operator::I16x8GtS,
            0x27 => Operator::I16x8GtU,
            0x28 => Operator::I16x8LeS,
            0x29 => Operator::I16x8LeU,
            0x2a => Operator::I16x8GeS,
            0x2b => Operator::I16x8GeU,
            0x2c => Operator::I32x4Eq,
            0x2d => Operator::I32x4Ne,
            0x2e => Operator::I32x4LtS,
            0x2f => Operator::I32x4LtU,
            0x30 => Operator::I32x4GtS,
            0x31 => Operator::I32x4GtU,
            0x32 => Operator::I32x4LeS,
            0x33 => Operator::I32x4LeU,
            0x34 => Operator::I32x4GeS,
            0x35 => Operator::I32x4GeU,
            0x40 => Operator::F32x4Eq,
            0x41 => Operator::F32x4Ne,
            0x42 => Operator::F32x4Lt,
            0x43 => Operator::F32x4Gt,
            0x44 => Operator::F32x4Le,
            0x45 => Operator::F32x4Ge,
            0x46 => Operator::F64x2Eq,
            0x47 => Operator::F64x2Ne,
            0x48 => Operator::F64x2Lt,
            0x49 => Operator::F64x2Gt,
            0x4a => Operator::F64x2Le,
            0x4b => Operator::F64x2Ge,
            0x4c => Operator::V128Not,
            0x4d => Operator::V128And,
            0x4e => Operator::V128Or,
            0x4f => Operator::V128Xor,
            0x50 => Operator::V128Bitselect,
            0x51 => Operator::I8x16Neg,
            0x52 => Operator::I8x16AnyTrue,
            0x53 => Operator::I8x16AllTrue,
            0x54 => Operator::I8x16Shl,
            0x55 => Operator::I8x16ShrS,
            0x56 => Operator::I8x16ShrU,
            0x57 => Operator::I8x16Add,
            0x58 => Operator::I8x16AddSaturateS,
            0x59 => Operator::I8x16AddSaturateU,
            0x5a => Operator::I8x16Sub,
            0x5b => Operator::I8x16SubSaturateS,
            0x5c => Operator::I8x16SubSaturateU,
            0x5d => Operator::I8x16Mul,
            0x62 => Operator::I16x8Neg,
            0x63 => Operator::I16x8AnyTrue,
            0x64 => Operator::I16x8AllTrue,
            0x65 => Operator::I16x8Shl,
            0x66 => Operator::I16x8ShrS,
            0x67 => Operator::I16x8ShrU,
            0x68 => Operator::I16x8Add,
            0x69 => Operator::I16x8AddSaturateS,
            0x6a => Operator::I16x8AddSaturateU,
            0x6b => Operator::I16x8Sub,
            0x6c => Operator::I16x8SubSaturateS,
            0x6d => Operator::I16x8SubSaturateU,
            0x6e => Operator::I16x8Mul,
            0x73 => Operator::I32x4Neg,
            0x74 => Operator::I32x4AnyTrue,
            0x75 => Operator::I32x4AllTrue,
            0x76 => Operator::I32x4Shl,
            0x77 => Operator::I32x4ShrS,
            0x78 => Operator::I32x4ShrU,
            0x79 => Operator::I32x4Add,
            0x7c => Operator::I32x4Sub,
            0x7f => Operator::I32x4Mul,
            0x84 => Operator::I64x2Neg,
            0x85 => Operator::I64x2AnyTrue,
            0x86 => Operator::I64x2AllTrue,
            0x87 => Operator::I64x2Shl,
            0x88 => Operator::I64x2ShrS,
            0x89 => Operator::I64x2ShrU,
            0x8a => Operator::I64x2Add,
            0x8d => Operator::I64x2Sub,
            0x90 => Operator::I64x2Mul,
            0x95 => Operator::F32x4Abs,
            0x96 => Operator::F32x4Neg,
            0x97 => Operator::F32x4Sqrt,
            0x9a => Operator::F32x4Add,
            0x9b => Operator::F32x4Sub,
            0x9c => Operator::F32x4Mul,
            0x9d => Operator::F32x4Div,
            0x9e => Operator::F32x4Min,
            0x9f => Operator::F32x4Max,
            0xa0 => Operator::F64x2Abs,
            0xa1 => Operator::F64x2Neg,
            0xa2 => Operator::F64x2Sqrt,
            0xa5 => Operator::F64x2Add,
            0xa6 => Operator::F64x2Sub,
            0xa7 => Operator::F64x2Mul,
            0xa8 => Operator::F64x2Div,
            0xa9 => Operator::F64x2Min,
            0xaa => Operator::F64x2Max,
            0xab => Operator::I32x4TruncSatF32x4S,
            0xac => Operator::I32x4TruncSatF32x4U,
            0xad => Operator::I64x2TruncSatF64x2S,
            0xae => Operator::I64x2TruncSatF64x2U,
            0xaf => Operator::F32x4ConvertI32x4S,
            0xb0 => Operator::F32x4ConvertI32x4U,
            0xb1 => Operator::F64x2ConvertI64x2S,
            0xb2 => Operator::F64x2ConvertI64x2U,
            0xc0 => Operator::V8x16Swizzle,
            0x03 | 0xc1 => {
                let mut lanes = [0 as SIMDLaneIndex; 16];
                for lane in &mut lanes {
                    *lane = self.read_lane_index(32)?
                }
                Operator::V8x16Shuffle { lanes }
            }
            0xc2 => Operator::V8x16LoadSplat {
                memarg: self.read_memarg_of_align(0)?,
            },
            0xc3 => Operator::V16x8LoadSplat {
                memarg: self.read_memarg_of_align(1)?,
            },
            0xc4 => Operator::V32x4LoadSplat {
                memarg: self.read_memarg_of_align(2)?,
            },
            0xc5 => Operator::V64x2LoadSplat {
                memarg: self.read_memarg_of_align(3)?,
            },
            0xc6 => Operator::I8x16NarrowI16x8S,
            0xc7 => Operator::I8x16NarrowI16x8U,
            0xc8 => Operator::I16x8NarrowI32x4S,
            0xc9 => Operator::I16x8NarrowI32x4U,
            0xca => Operator::I16x8WidenLowI8x16S,
            0xcb => Operator::I16x8WidenHighI8x16S,
            0xcc => Operator::I16x8WidenLowI8x16U,
            0xcd => Operator::I16x8WidenHighI8x16U,
            0xce => Operator::I32x4WidenLowI16x8S,
            0xcf => Operator::I32x4WidenHighI16x8S,
            0xd0 => Operator::I32x4WidenLowI16x8U,
            0xd1 => Operator::I32x4WidenHighI16x8U,
            0xd2 => Operator::I16x8Load8x8S {
                memarg: self.read_memarg_of_align(3)?,
            },
            0xd3 => Operator::I16x8Load8x8U {
                memarg: self.read_memarg_of_align(3)?,
            },
            0xd4 => Operator::I32x4Load16x4S {
                memarg: self.read_memarg_of_align(3)?,
            },
            0xd5 => Operator::I32x4Load16x4U {
                memarg: self.read_memarg_of_align(3)?,
            },
            0xd6 => Operator::I64x2Load32x2S {
                memarg: self.read_memarg_of_align(3)?,
            },
            0xd7 => Operator::I64x2Load32x2U {
                memarg: self.read_memarg_of_align(3)?,
            },
            0xd8 => Operator::V128AndNot,
            0xd9 => Operator::I8x16RoundingAverageU,
            0xda => Operator::I16x8RoundingAverageU,
            _ => {
                return Err(BinaryReaderError {
                    message: "Unknown 0xfd opcode",
                    offset: self.original_position() - 1,
                });
            }
        })
    }

    pub(crate) fn read_file_header(&mut self) -> Result<u32> {
        let magic_number = self.read_bytes(4)?;
        if magic_number != WASM_MAGIC_NUMBER {
            return Err(BinaryReaderError {
                message: "Bad magic number",
                offset: self.original_position() - 4,
            });
        }
        let version = self.read_u32()?;
        if version != WASM_SUPPORTED_VERSION && version != WASM_EXPERIMENTAL_VERSION {
            return Err(BinaryReaderError {
                message: "Bad version number",
                offset: self.original_position() - 4,
            });
        }
        Ok(version)
    }

    pub(crate) fn read_section_header(&mut self) -> Result<SectionHeader<'a>> {
        let id_position = self.position;
        let id = self.read_var_u7()?;
        let payload_len = self.read_var_u32()? as usize;
        let payload_start = self.position;
        let code = self.read_section_code(id, id_position)?;
        Ok(SectionHeader {
            code,
            payload_start,
            payload_len,
        })
    }

    pub(crate) fn read_name_type(&mut self) -> Result<NameType> {
        let code = self.read_var_u7()?;
        match code {
            0 => Ok(NameType::Module),
            1 => Ok(NameType::Function),
            2 => Ok(NameType::Local),
            _ => Err(BinaryReaderError {
                message: "Invalid name type",
                offset: self.original_position() - 1,
            }),
        }
    }

    pub(crate) fn read_linking_type(&mut self) -> Result<LinkingType> {
        let ty = self.read_var_u32()?;
        Ok(match ty {
            1 => LinkingType::StackPointer(self.read_var_u32()?),
            _ => {
                return Err(BinaryReaderError {
                    message: "Invalid linking type",
                    offset: self.original_position() - 1,
                });
            }
        })
    }

    pub(crate) fn read_reloc_type(&mut self) -> Result<RelocType> {
        let code = self.read_var_u7()?;
        match code {
            0 => Ok(RelocType::FunctionIndexLEB),
            1 => Ok(RelocType::TableIndexSLEB),
            2 => Ok(RelocType::TableIndexI32),
            3 => Ok(RelocType::GlobalAddrLEB),
            4 => Ok(RelocType::GlobalAddrSLEB),
            5 => Ok(RelocType::GlobalAddrI32),
            6 => Ok(RelocType::TypeIndexLEB),
            7 => Ok(RelocType::GlobalIndexLEB),
            _ => Err(BinaryReaderError {
                message: "Invalid reloc type",
                offset: self.original_position() - 1,
            }),
        }
    }

    pub(crate) fn skip_init_expr(&mut self) -> Result<()> {
        // TODO add skip_operator() method and/or validate init_expr operators.
        loop {
            if let Operator::End = self.read_operator()? {
                return Ok(());
            }
        }
    }
}

impl<'a> BrTable<'a> {
    /// Returns the number of `br_table` entries, not including the default
    /// label
    pub fn len(&self) -> usize {
        self.cnt
    }

    /// Returns whether `BrTable` doesn't have any labels apart from the default one.
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Reads br_table entries.
    ///
    /// # Examples
    /// ```rust
    /// let buf = vec![0x0e, 0x02, 0x01, 0x02, 0x00];
    /// let mut reader = wasmparser::BinaryReader::new(&buf);
    /// let op = reader.read_operator().unwrap();
    /// if let wasmparser::Operator::BrTable { ref table } = op {
    ///     let br_table_depths = table.read_table().unwrap();
    ///     assert!(br_table_depths.0 == vec![1,2].into_boxed_slice() &&
    ///             br_table_depths.1 == 0);
    /// } else {
    ///     unreachable!();
    /// }
    /// ```
    pub fn read_table(&self) -> Result<(Box<[u32]>, u32)> {
        let mut reader = BinaryReader::new(self.buffer);
        let mut table = Vec::new();
        while !reader.eof() {
            table.push(reader.read_var_u32()?);
        }
        let default_target = table.pop().ok_or_else(|| BinaryReaderError {
            message: "br_table missing default target",
            offset: reader.original_position(),
        })?;
        Ok((table.into_boxed_slice(), default_target))
    }
}

/// Iterator for `BrTable`.
///
/// #Examples
/// ```rust
/// let buf = vec![0x0e, 0x02, 0x01, 0x02, 0x00];
/// let mut reader = wasmparser::BinaryReader::new(&buf);
/// let op = reader.read_operator().unwrap();
/// if let wasmparser::Operator::BrTable { ref table } = op {
///     for depth in table {
///         println!("BrTable depth: {}", depth);
///     }
/// }
/// ```
#[derive(Clone, Debug)]
pub struct BrTableIterator<'a> {
    reader: BinaryReader<'a>,
}

impl<'a> IntoIterator for &'a BrTable<'a> {
    type Item = u32;
    type IntoIter = BrTableIterator<'a>;

    fn into_iter(self) -> Self::IntoIter {
        BrTableIterator {
            reader: BinaryReader::new(self.buffer),
        }
    }
}

impl<'a> Iterator for BrTableIterator<'a> {
    type Item = u32;

    fn next(&mut self) -> Option<u32> {
        if self.reader.eof() {
            return None;
        }
        self.reader.read_var_u32().ok()
    }
}
