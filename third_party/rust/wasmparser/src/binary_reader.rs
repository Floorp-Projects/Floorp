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

use crate::{limits::*, *};
use std::convert::TryInto;
use std::error::Error;
use std::fmt;
use std::ops::Range;
use std::str;

fn is_name(name: &str, expected: &'static str) -> bool {
    name == expected
}

fn is_name_prefix(name: &str, prefix: &'static str) -> bool {
    name.starts_with(prefix)
}

const WASM_MAGIC_NUMBER: &[u8; 4] = b"\0asm";

/// A binary reader for WebAssembly modules.
#[derive(Debug, Clone)]
pub struct BinaryReaderError {
    // Wrap the actual error data in a `Box` so that the error is just one
    // word. This means that we can continue returning small `Result`s in
    // registers.
    pub(crate) inner: Box<BinaryReaderErrorInner>,
}

#[derive(Debug, Clone)]
pub(crate) struct BinaryReaderErrorInner {
    pub(crate) message: String,
    pub(crate) offset: usize,
    pub(crate) needed_hint: Option<usize>,
}

/// The result for `BinaryReader` operations.
pub type Result<T, E = BinaryReaderError> = std::result::Result<T, E>;

impl Error for BinaryReaderError {}

impl fmt::Display for BinaryReaderError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "{} (at offset 0x{:x})",
            self.inner.message, self.inner.offset
        )
    }
}

impl BinaryReaderError {
    pub(crate) fn new(message: impl Into<String>, offset: usize) -> Self {
        let message = message.into();
        BinaryReaderError {
            inner: Box::new(BinaryReaderErrorInner {
                message,
                offset,
                needed_hint: None,
            }),
        }
    }

    pub(crate) fn eof(offset: usize, needed_hint: usize) -> Self {
        BinaryReaderError {
            inner: Box::new(BinaryReaderErrorInner {
                message: "unexpected end-of-file".to_string(),
                offset,
                needed_hint: Some(needed_hint),
            }),
        }
    }

    /// Get this error's message.
    pub fn message(&self) -> &str {
        &self.inner.message
    }

    /// Get the offset within the Wasm binary where the error occurred.
    pub fn offset(&self) -> usize {
        self.inner.offset
    }
}

/// A binary reader of the WebAssembly structures and types.
#[derive(Clone, Debug, Hash)]
pub struct BinaryReader<'a> {
    pub(crate) buffer: &'a [u8],
    pub(crate) position: usize,
    pub(crate) original_offset: usize,
    allow_memarg64: bool,
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
            allow_memarg64: false,
        }
    }

    /// Constructs a `BinaryReader` with an explicit starting offset.
    pub fn new_with_offset(data: &[u8], original_offset: usize) -> BinaryReader {
        BinaryReader {
            buffer: data,
            position: 0,
            original_offset,
            allow_memarg64: false,
        }
    }

    /// Gets the original position of the binary reader.
    pub fn original_position(&self) -> usize {
        self.original_offset + self.position
    }

    /// Whether or not to allow 64-bit memory arguments in functions.
    ///
    /// This is intended to be `true` when support for the memory64
    /// WebAssembly proposal is also enabled.
    pub fn allow_memarg64(&mut self, allow: bool) {
        self.allow_memarg64 = allow;
    }

    /// Returns a range from the starting offset to the end of the buffer.
    pub fn range(&self) -> Range<usize> {
        self.original_offset..self.original_offset + self.buffer.len()
    }

    pub(crate) fn remaining_buffer(&self) -> &'a [u8] {
        &self.buffer[self.position..]
    }

    fn ensure_has_byte(&self) -> Result<()> {
        if self.position < self.buffer.len() {
            Ok(())
        } else {
            Err(BinaryReaderError::eof(self.original_position(), 1))
        }
    }

    pub(crate) fn ensure_has_bytes(&self, len: usize) -> Result<()> {
        if self.position + len <= self.buffer.len() {
            Ok(())
        } else {
            let hint = self.position + len - self.buffer.len();
            Err(BinaryReaderError::eof(self.original_position(), hint))
        }
    }

    pub(crate) fn read_u7(&mut self) -> Result<u8> {
        let b = self.read_u8()?;
        if (b & 0x80) != 0 {
            return Err(BinaryReaderError::new(
                "invalid u7",
                self.original_position() - 1,
            ));
        }
        Ok(b)
    }

    /// Reads a core WebAssembly value type from the binary reader.
    pub fn read_val_type(&mut self) -> Result<ValType> {
        match Self::val_type_from_byte(self.peek()?) {
            Some(ty) => {
                self.position += 1;
                Ok(ty)
            }
            None => Err(BinaryReaderError::new(
                "invalid value type",
                self.original_position(),
            )),
        }
    }

    pub(crate) fn read_component_start(&mut self) -> Result<ComponentStartFunction> {
        let func_index = self.read_var_u32()?;
        let size = self.read_size(MAX_WASM_START_ARGS, "start function arguments")?;
        Ok(ComponentStartFunction {
            func_index,
            arguments: (0..size)
                .map(|_| self.read_var_u32())
                .collect::<Result<_>>()?,
        })
    }

    fn external_kind_from_byte(byte: u8, offset: usize) -> Result<ExternalKind> {
        match byte {
            0x00 => Ok(ExternalKind::Func),
            0x01 => Ok(ExternalKind::Table),
            0x02 => Ok(ExternalKind::Memory),
            0x03 => Ok(ExternalKind::Global),
            0x04 => Ok(ExternalKind::Tag),
            x => Err(Self::invalid_leading_byte_error(x, "external kind", offset)),
        }
    }

    pub(crate) fn read_external_kind(&mut self) -> Result<ExternalKind> {
        let offset = self.original_position();
        Self::external_kind_from_byte(self.read_u8()?, offset)
    }

    fn component_external_kind_from_bytes(
        byte1: u8,
        byte2: Option<u8>,
        offset: usize,
    ) -> Result<ComponentExternalKind> {
        Ok(match byte1 {
            0x00 => match byte2.unwrap() {
                0x11 => ComponentExternalKind::Module,
                x => {
                    return Err(Self::invalid_leading_byte_error(
                        x,
                        "component external kind",
                        offset + 1,
                    ))
                }
            },
            0x01 => ComponentExternalKind::Func,
            0x02 => ComponentExternalKind::Value,
            0x03 => ComponentExternalKind::Type,
            0x04 => ComponentExternalKind::Component,
            0x05 => ComponentExternalKind::Instance,
            x => {
                return Err(Self::invalid_leading_byte_error(
                    x,
                    "component external kind",
                    offset,
                ))
            }
        })
    }

    pub(crate) fn read_component_external_kind(&mut self) -> Result<ComponentExternalKind> {
        let offset = self.original_position();
        let byte1 = self.read_u8()?;
        let byte2 = if byte1 == 0x00 {
            Some(self.read_u8()?)
        } else {
            None
        };

        Self::component_external_kind_from_bytes(byte1, byte2, offset)
    }

    pub(crate) fn read_func_type(&mut self) -> Result<FuncType> {
        let params_len = self.read_size(MAX_WASM_FUNCTION_PARAMS, "function params")?;
        let mut params = Vec::with_capacity(params_len);
        for _ in 0..params_len {
            params.push(self.read_val_type()?);
        }
        let returns_len = self.read_size(MAX_WASM_FUNCTION_RETURNS, "function returns")?;
        let mut returns = Vec::with_capacity(returns_len);
        for _ in 0..returns_len {
            returns.push(self.read_val_type()?);
        }
        Ok(FuncType {
            params: params.into_boxed_slice(),
            returns: returns.into_boxed_slice(),
        })
    }

    pub(crate) fn read_type(&mut self) -> Result<Type> {
        Ok(match self.read_u8()? {
            0x60 => Type::Func(self.read_func_type()?),
            x => return self.invalid_leading_byte(x, "type"),
        })
    }

    pub(crate) fn read_core_type(&mut self) -> Result<CoreType<'a>> {
        Ok(match self.read_u8()? {
            0x60 => CoreType::Func(self.read_func_type()?),
            0x50 => {
                let size = self.read_size(MAX_WASM_MODULE_TYPE_DECLS, "module type declaration")?;
                CoreType::Module(
                    (0..size)
                        .map(|_| self.read_module_type_decl())
                        .collect::<Result<_>>()?,
                )
            }
            x => return self.invalid_leading_byte(x, "core type"),
        })
    }

    pub(crate) fn read_component_type(&mut self) -> Result<ComponentType<'a>> {
        Ok(match self.read_u8()? {
            0x40 => {
                let params_size =
                    self.read_size(MAX_WASM_FUNCTION_PARAMS, "function parameters")?;
                let params = (0..params_size)
                    .map(|_| {
                        Ok((
                            self.read_optional_string()?,
                            self.read_component_val_type()?,
                        ))
                    })
                    .collect::<Result<_>>()?;
                ComponentType::Func(ComponentFuncType {
                    params,
                    result: self.read_component_val_type()?,
                })
            }
            0x41 => {
                let size =
                    self.read_size(MAX_WASM_COMPONENT_TYPE_DECLS, "component type declaration")?;
                ComponentType::Component(
                    (0..size)
                        .map(|_| self.read_component_type_decl())
                        .collect::<Result<_>>()?,
                )
            }
            0x42 => {
                let size =
                    self.read_size(MAX_WASM_INSTANCE_TYPE_DECLS, "instance type declaration")?;
                ComponentType::Instance(
                    (0..size)
                        .map(|_| self.read_instance_type_decl())
                        .collect::<Result<_>>()?,
                )
            }
            x => {
                if let Some(ty) = Self::primitive_val_type_from_byte(x) {
                    ComponentType::Defined(ComponentDefinedType::Primitive(ty))
                } else {
                    ComponentType::Defined(self.read_component_defined_type(x)?)
                }
            }
        })
    }

    pub(crate) fn read_module_type_decl(&mut self) -> Result<ModuleTypeDeclaration<'a>> {
        Ok(match self.read_u8()? {
            0x00 => ModuleTypeDeclaration::Import(self.read_import()?),
            0x01 => ModuleTypeDeclaration::Type(self.read_type()?),
            0x02 => ModuleTypeDeclaration::Alias(self.read_alias()?),
            0x03 => ModuleTypeDeclaration::Export {
                name: self.read_string()?,
                ty: self.read_type_ref()?,
            },
            x => return self.invalid_leading_byte(x, "type definition"),
        })
    }

    pub(crate) fn read_component_type_decl(&mut self) -> Result<ComponentTypeDeclaration<'a>> {
        // Component types are effectively instance types with the additional
        // variant of imports; check for imports here or delegate to
        // `read_instance_type_decl` with the appropriate conversions.
        if self.peek()? == 0x03 {
            self.position += 1;
            return Ok(ComponentTypeDeclaration::Import(
                self.read_component_import()?,
            ));
        }

        Ok(match self.read_instance_type_decl()? {
            InstanceTypeDeclaration::CoreType(t) => ComponentTypeDeclaration::CoreType(t),
            InstanceTypeDeclaration::Type(t) => ComponentTypeDeclaration::Type(t),
            InstanceTypeDeclaration::Alias(a) => ComponentTypeDeclaration::Alias(a),
            InstanceTypeDeclaration::Export { name, ty } => {
                ComponentTypeDeclaration::Export { name, ty }
            }
        })
    }

    pub(crate) fn read_instance_type_decl(&mut self) -> Result<InstanceTypeDeclaration<'a>> {
        Ok(match self.read_u8()? {
            0x00 => InstanceTypeDeclaration::CoreType(self.read_core_type()?),
            0x01 => InstanceTypeDeclaration::Type(self.read_component_type()?),
            0x02 => InstanceTypeDeclaration::Alias(self.read_component_alias()?),
            0x04 => InstanceTypeDeclaration::Export {
                name: self.read_string()?,
                ty: self.read_component_type_ref()?,
            },
            x => return self.invalid_leading_byte(x, "component or instance type declaration"),
        })
    }

    fn primitive_val_type_from_byte(byte: u8) -> Option<PrimitiveValType> {
        Some(match byte {
            0x7f => PrimitiveValType::Unit,
            0x7e => PrimitiveValType::Bool,
            0x7d => PrimitiveValType::S8,
            0x7c => PrimitiveValType::U8,
            0x7b => PrimitiveValType::S16,
            0x7a => PrimitiveValType::U16,
            0x79 => PrimitiveValType::S32,
            0x78 => PrimitiveValType::U32,
            0x77 => PrimitiveValType::S64,
            0x76 => PrimitiveValType::U64,
            0x75 => PrimitiveValType::Float32,
            0x74 => PrimitiveValType::Float64,
            0x73 => PrimitiveValType::Char,
            0x72 => PrimitiveValType::String,
            _ => return None,
        })
    }

    fn read_variant_case(&mut self) -> Result<VariantCase<'a>> {
        Ok(VariantCase {
            name: self.read_string()?,
            ty: self.read_component_val_type()?,
            refines: match self.read_u8()? {
                0x0 => None,
                0x1 => Some(self.read_var_u32()?),
                x => return self.invalid_leading_byte(x, "variant case default"),
            },
        })
    }

    fn read_component_val_type(&mut self) -> Result<ComponentValType> {
        if let Some(ty) = Self::primitive_val_type_from_byte(self.peek()?) {
            self.position += 1;
            return Ok(ComponentValType::Primitive(ty));
        }

        Ok(ComponentValType::Type(self.read_var_s33()? as u32))
    }

    fn read_component_defined_type(&mut self, byte: u8) -> Result<ComponentDefinedType<'a>> {
        Ok(match byte {
            0x71 => {
                let size = self.read_size(MAX_WASM_RECORD_FIELDS, "record field")?;
                ComponentDefinedType::Record(
                    (0..size)
                        .map(|_| Ok((self.read_string()?, self.read_component_val_type()?)))
                        .collect::<Result<_>>()?,
                )
            }
            0x70 => {
                let size = self.read_size(MAX_WASM_VARIANT_CASES, "variant cases")?;
                ComponentDefinedType::Variant(
                    (0..size)
                        .map(|_| self.read_variant_case())
                        .collect::<Result<_>>()?,
                )
            }
            0x6f => ComponentDefinedType::List(self.read_component_val_type()?),
            0x6e => {
                let size = self.read_size(MAX_WASM_TUPLE_TYPES, "tuple types")?;
                ComponentDefinedType::Tuple(
                    (0..size)
                        .map(|_| self.read_component_val_type())
                        .collect::<Result<_>>()?,
                )
            }
            0x6d => {
                let size = self.read_size(MAX_WASM_FLAG_NAMES, "flag names")?;
                ComponentDefinedType::Flags(
                    (0..size)
                        .map(|_| self.read_string())
                        .collect::<Result<_>>()?,
                )
            }
            0x6c => {
                let size = self.read_size(MAX_WASM_ENUM_CASES, "enum cases")?;
                ComponentDefinedType::Enum(
                    (0..size)
                        .map(|_| self.read_string())
                        .collect::<Result<_>>()?,
                )
            }
            0x6b => {
                let size = self.read_size(MAX_WASM_UNION_TYPES, "union types")?;
                ComponentDefinedType::Union(
                    (0..size)
                        .map(|_| self.read_component_val_type())
                        .collect::<Result<_>>()?,
                )
            }
            0x6a => ComponentDefinedType::Option(self.read_component_val_type()?),
            0x69 => ComponentDefinedType::Expected {
                ok: self.read_component_val_type()?,
                error: self.read_component_val_type()?,
            },
            x => return self.invalid_leading_byte(x, "component defined type"),
        })
    }

    pub(crate) fn read_export(&mut self) -> Result<Export<'a>> {
        Ok(Export {
            name: self.read_string()?,
            kind: self.read_external_kind()?,
            index: self.read_var_u32()?,
        })
    }

    pub(crate) fn read_component_export(&mut self) -> Result<ComponentExport<'a>> {
        Ok(ComponentExport {
            name: self.read_string()?,
            kind: self.read_component_external_kind()?,
            index: self.read_var_u32()?,
        })
    }

    pub(crate) fn read_import(&mut self) -> Result<Import<'a>> {
        Ok(Import {
            module: self.read_string()?,
            name: self.read_string()?,
            ty: self.read_type_ref()?,
        })
    }

    pub(crate) fn read_component_import(&mut self) -> Result<ComponentImport<'a>> {
        Ok(ComponentImport {
            name: self.read_string()?,
            ty: self.read_component_type_ref()?,
        })
    }

    pub(crate) fn read_component_type_ref(&mut self) -> Result<ComponentTypeRef> {
        Ok(match self.read_component_external_kind()? {
            ComponentExternalKind::Module => ComponentTypeRef::Module(self.read_var_u32()?),
            ComponentExternalKind::Func => ComponentTypeRef::Func(self.read_var_u32()?),
            ComponentExternalKind::Value => {
                ComponentTypeRef::Value(self.read_component_val_type()?)
            }
            ComponentExternalKind::Type => {
                ComponentTypeRef::Type(self.read_type_bounds()?, self.read_var_u32()?)
            }
            ComponentExternalKind::Instance => ComponentTypeRef::Instance(self.read_var_u32()?),
            ComponentExternalKind::Component => ComponentTypeRef::Component(self.read_var_u32()?),
        })
    }

    pub(crate) fn read_type_bounds(&mut self) -> Result<TypeBounds> {
        Ok(match self.read_u8()? {
            0x00 => TypeBounds::Eq,
            x => return self.invalid_leading_byte(x, "type bound"),
        })
    }

    pub(crate) fn read_canonical_func(&mut self) -> Result<CanonicalFunction> {
        Ok(match self.read_u8()? {
            0x00 => match self.read_u8()? {
                0x00 => CanonicalFunction::Lift {
                    core_func_index: self.read_var_u32()?,
                    options: (0..self
                        .read_size(MAX_WASM_CANONICAL_OPTIONS, "canonical options")?)
                        .map(|_| self.read_canonical_option())
                        .collect::<Result<_>>()?,
                    type_index: self.read_var_u32()?,
                },
                x => return self.invalid_leading_byte(x, "canonical function lift"),
            },
            0x01 => match self.read_u8()? {
                0x00 => CanonicalFunction::Lower {
                    func_index: self.read_var_u32()?,
                    options: (0..self
                        .read_size(MAX_WASM_CANONICAL_OPTIONS, "canonical options")?)
                        .map(|_| self.read_canonical_option())
                        .collect::<Result<_>>()?,
                },
                x => return self.invalid_leading_byte(x, "canonical function lower"),
            },
            x => return self.invalid_leading_byte(x, "canonical function"),
        })
    }

    pub(crate) fn read_canonical_option(&mut self) -> Result<CanonicalOption> {
        Ok(match self.read_u8()? {
            0x00 => CanonicalOption::UTF8,
            0x01 => CanonicalOption::UTF16,
            0x02 => CanonicalOption::CompactUTF16,
            0x03 => CanonicalOption::Memory(self.read_var_u32()?),
            0x04 => CanonicalOption::Realloc(self.read_var_u32()?),
            0x05 => CanonicalOption::PostReturn(self.read_var_u32()?),
            x => return self.invalid_leading_byte(x, "canonical option"),
        })
    }

    pub(crate) fn read_instance(&mut self) -> Result<Instance<'a>> {
        Ok(match self.read_u8()? {
            0x00 => Instance::Instantiate {
                module_index: self.read_var_u32()?,
                args: (0..self
                    .read_size(MAX_WASM_INSTANTIATION_ARGS, "core instantiation arguments")?)
                    .map(|_| self.read_instantiation_arg())
                    .collect::<Result<_>>()?,
            },
            0x01 => Instance::FromExports(
                (0..self
                    .read_size(MAX_WASM_INSTANTIATION_EXPORTS, "core instantiation exports")?)
                    .map(|_| self.read_export())
                    .collect::<Result<_>>()?,
            ),
            x => return self.invalid_leading_byte(x, "core instance"),
        })
    }

    pub(crate) fn read_component_instance(&mut self) -> Result<ComponentInstance<'a>> {
        Ok(match self.read_u8()? {
            0x00 => ComponentInstance::Instantiate {
                component_index: self.read_var_u32()?,
                args: (0..self
                    .read_size(MAX_WASM_INSTANTIATION_ARGS, "instantiation arguments")?)
                    .map(|_| self.read_component_instantiation_arg())
                    .collect::<Result<_>>()?,
            },
            0x01 => ComponentInstance::FromExports(
                (0..self.read_size(MAX_WASM_INSTANTIATION_EXPORTS, "instantiation exports")?)
                    .map(|_| self.read_component_export())
                    .collect::<Result<_>>()?,
            ),
            x => return self.invalid_leading_byte(x, "instance"),
        })
    }

    pub(crate) fn read_instantiation_arg_kind(&mut self) -> Result<InstantiationArgKind> {
        Ok(match self.read_u8()? {
            0x12 => InstantiationArgKind::Instance,
            x => return self.invalid_leading_byte(x, "instantiation arg kind"),
        })
    }

    pub(crate) fn read_instantiation_arg(&mut self) -> Result<InstantiationArg<'a>> {
        Ok(InstantiationArg {
            name: self.read_string()?,
            kind: self.read_instantiation_arg_kind()?,
            index: self.read_var_u32()?,
        })
    }

    pub(crate) fn read_component_instantiation_arg(
        &mut self,
    ) -> Result<ComponentInstantiationArg<'a>> {
        Ok(ComponentInstantiationArg {
            name: self.read_string()?,
            kind: self.read_component_external_kind()?,
            index: self.read_var_u32()?,
        })
    }

    pub(crate) fn read_alias(&mut self) -> Result<Alias<'a>> {
        let offset = self.original_position();
        let kind = self.read_u8()?;

        Ok(match self.read_u8()? {
            0x00 => Alias::InstanceExport {
                kind: Self::external_kind_from_byte(kind, offset)?,
                instance_index: self.read_var_u32()?,
                name: self.read_string()?,
            },
            0x01 => Alias::Outer {
                kind: match kind {
                    0x10 => OuterAliasKind::Type,
                    x => {
                        return Err(Self::invalid_leading_byte_error(
                            x,
                            "outer alias kind",
                            offset,
                        ))
                    }
                },
                count: self.read_var_u32()?,
                index: self.read_var_u32()?,
            },
            x => return self.invalid_leading_byte(x, "alias"),
        })
    }

    fn component_outer_alias_kind_from_bytes(
        byte1: u8,
        byte2: Option<u8>,
        offset: usize,
    ) -> Result<ComponentOuterAliasKind> {
        Ok(match byte1 {
            0x00 => match byte2.unwrap() {
                0x10 => ComponentOuterAliasKind::CoreType,
                0x11 => ComponentOuterAliasKind::CoreModule,
                x => {
                    return Err(Self::invalid_leading_byte_error(
                        x,
                        "component outer alias kind",
                        offset + 1,
                    ))
                }
            },
            0x03 => ComponentOuterAliasKind::Type,
            0x04 => ComponentOuterAliasKind::Component,
            x => {
                return Err(Self::invalid_leading_byte_error(
                    x,
                    "component outer alias kind",
                    offset,
                ))
            }
        })
    }

    pub(crate) fn read_component_alias(&mut self) -> Result<ComponentAlias<'a>> {
        // We don't know what type of alias it is yet, so just read the sort bytes
        let offset = self.original_position();
        let byte1 = self.read_u8()?;
        let byte2 = if byte1 == 0x00 {
            Some(self.read_u8()?)
        } else {
            None
        };

        Ok(match self.read_u8()? {
            0x00 => ComponentAlias::InstanceExport {
                kind: Self::component_external_kind_from_bytes(byte1, byte2, offset)?,
                instance_index: self.read_var_u32()?,
                name: self.read_string()?,
            },
            0x01 => ComponentAlias::Outer {
                kind: Self::component_outer_alias_kind_from_bytes(byte1, byte2, offset)?,
                count: self.read_var_u32()?,
                index: self.read_var_u32()?,
            },
            x => return self.invalid_leading_byte(x, "alias"),
        })
    }

    pub(crate) fn read_type_ref(&mut self) -> Result<TypeRef> {
        Ok(match self.read_external_kind()? {
            ExternalKind::Func => TypeRef::Func(self.read_var_u32()?),
            ExternalKind::Table => TypeRef::Table(self.read_table_type()?),
            ExternalKind::Memory => TypeRef::Memory(self.read_memory_type()?),
            ExternalKind::Global => TypeRef::Global(self.read_global_type()?),
            ExternalKind::Tag => TypeRef::Tag(self.read_tag_type()?),
        })
    }

    pub(crate) fn read_table_type(&mut self) -> Result<TableType> {
        let element_type = self.read_val_type()?;
        let has_max = match self.read_u8()? {
            0x00 => false,
            0x01 => true,
            _ => {
                return Err(BinaryReaderError::new(
                    "invalid table resizable limits flags",
                    self.original_position() - 1,
                ))
            }
        };
        let initial = self.read_var_u32()?;
        let maximum = if has_max {
            Some(self.read_var_u32()?)
        } else {
            None
        };
        Ok(TableType {
            element_type,
            initial,
            maximum,
        })
    }

    pub(crate) fn read_memory_type(&mut self) -> Result<MemoryType> {
        let pos = self.original_position();
        let flags = self.read_u8()?;
        if (flags & !0b111) != 0 {
            return Err(BinaryReaderError::new("invalid memory limits flags", pos));
        }

        let memory64 = flags & 0b100 != 0;
        let shared = flags & 0b010 != 0;
        let has_max = flags & 0b001 != 0;
        Ok(MemoryType {
            memory64,
            shared,
            // FIXME(WebAssembly/memory64#21) as currently specified if the
            // `shared` flag is set we should be reading a 32-bit limits field
            // here. That seems a bit odd to me at the time of this writing so
            // I've taken the liberty of reading a 64-bit limits field in those
            // situations. I suspect that this is a typo in the spec, but if not
            // we'll need to update this to read a 32-bit limits field when the
            // shared flag is set.
            initial: if memory64 {
                self.read_var_u64()?
            } else {
                self.read_var_u32()?.into()
            },
            maximum: if !has_max {
                None
            } else if memory64 {
                Some(self.read_var_u64()?)
            } else {
                Some(self.read_var_u32()?.into())
            },
        })
    }

    pub(crate) fn read_tag_type(&mut self) -> Result<TagType> {
        let attribute = self.read_u8()?;
        if attribute != 0 {
            return Err(BinaryReaderError::new(
                "invalid tag attributes",
                self.original_position() - 1,
            ));
        }
        Ok(TagType {
            kind: TagKind::Exception,
            func_type_idx: self.read_var_u32()?,
        })
    }

    pub(crate) fn read_global_type(&mut self) -> Result<GlobalType> {
        Ok(GlobalType {
            content_type: self.read_val_type()?,
            mutable: match self.read_u8()? {
                0x00 => false,
                0x01 => true,
                _ => {
                    return Err(BinaryReaderError::new(
                        "malformed mutability",
                        self.original_position() - 1,
                    ))
                }
            },
        })
    }

    // Reads a variable-length 32-bit size from the byte stream while checking
    // against a limit.
    fn read_size(&mut self, limit: usize, desc: &str) -> Result<usize> {
        let size = self.read_var_u32()? as usize;
        if size > limit {
            return Err(BinaryReaderError::new(
                format!("{} size is out of bounds", desc),
                self.original_position() - 4,
            ));
        }
        Ok(size)
    }

    fn read_first_byte_and_var_u32(&mut self) -> Result<(u8, u32)> {
        let pos = self.position;
        let val = self.read_var_u32()?;
        Ok((self.buffer[pos], val))
    }

    fn read_memarg(&mut self) -> Result<MemoryImmediate> {
        let flags_pos = self.original_position();
        let mut flags = self.read_var_u32()?;
        let memory = if flags & (1 << 6) != 0 {
            flags ^= 1 << 6;
            self.read_var_u32()?
        } else {
            0
        };
        let align = if flags >= (1 << 6) {
            return Err(BinaryReaderError::new("alignment too large", flags_pos));
        } else {
            flags as u8
        };
        let offset = if self.allow_memarg64 {
            self.read_var_u64()?
        } else {
            u64::from(self.read_var_u32()?)
        };
        Ok(MemoryImmediate {
            align,
            offset,
            memory,
        })
    }

    pub(crate) fn read_section_code(&mut self, id: u8, offset: usize) -> Result<SectionCode<'a>> {
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
            13 => Ok(SectionCode::Tag),
            _ => Err(BinaryReaderError::new("invalid section code", offset)),
        }
    }

    fn read_br_table(&mut self) -> Result<BrTable<'a>> {
        let cnt = self.read_size(MAX_WASM_BR_TABLE_SIZE, "br_table")?;
        let start = self.position;
        for _ in 0..cnt {
            self.read_var_u32()?;
        }
        let end = self.position;
        let default = self.read_var_u32()?;
        Ok(BrTable {
            reader: BinaryReader::new_with_offset(&self.buffer[start..end], start),
            cnt: cnt as u32,
            default,
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

    /// Advances the `BinaryReader` a single byte.
    ///
    /// # Errors
    ///
    /// If `BinaryReader` has no bytes remaining.
    pub fn read_u8(&mut self) -> Result<u8> {
        self.ensure_has_byte()?;
        let b = self.buffer[self.position];
        self.position += 1;
        Ok(b)
    }

    /// Advances the `BinaryReader` up to four bytes to parse a variable
    /// length integer as a `u32`.
    ///
    /// # Errors
    ///
    /// If `BinaryReader` has less than one or up to four bytes remaining, or
    /// the integer is larger than 32 bits.
    pub fn read_var_u32(&mut self) -> Result<u32> {
        // Optimization for single byte i32.
        let byte = self.read_u8()?;
        if (byte & 0x80) == 0 {
            return Ok(byte as u32);
        }

        let mut result = (byte & 0x7F) as u32;
        let mut shift = 7;
        loop {
            let byte = self.read_u8()?;
            result |= ((byte & 0x7F) as u32) << shift;
            if shift >= 25 && (byte >> (32 - shift)) != 0 {
                let msg = if byte & 0x80 != 0 {
                    "invalid var_u32: integer representation too long"
                } else {
                    "invalid var_u32: integer too large"
                };
                // The continuation bit or unused bits are set.
                return Err(BinaryReaderError::new(msg, self.original_position() - 1));
            }
            shift += 7;
            if (byte & 0x80) == 0 {
                break;
            }
        }
        Ok(result)
    }

    /// Advances the `BinaryReader` up to four bytes to parse a variable
    /// length integer as a `u64`.
    ///
    /// # Errors
    ///
    /// If `BinaryReader` has less than one or up to eight bytes remaining, or
    /// the integer is larger than 64 bits.
    pub fn read_var_u64(&mut self) -> Result<u64> {
        // Optimization for single byte u64.
        let byte = u64::from(self.read_u8()?);
        if (byte & 0x80) == 0 {
            return Ok(byte);
        }

        let mut result = byte & 0x7F;
        let mut shift = 7;
        loop {
            let byte = u64::from(self.read_u8()?);
            result |= (byte & 0x7F) << shift;
            if shift >= 57 && (byte >> (64 - shift)) != 0 {
                let msg = if byte & 0x80 != 0 {
                    "invalid var_u64: integer representation too long"
                } else {
                    "invalid var_u64: integer too large"
                };
                // The continuation bit or unused bits are set.
                return Err(BinaryReaderError::new(msg, self.original_position() - 1));
            }
            shift += 7;
            if (byte & 0x80) == 0 {
                break;
            }
        }
        Ok(result)
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
            return Err(BinaryReaderError::new(
                "string size out of bounds",
                self.original_position() - 1,
            ));
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
                    let msg = if continuation_bit {
                        "invalid var_i32: integer representation too long"
                    } else {
                        "invalid var_i32: integer too large"
                    };
                    return Err(BinaryReaderError::new(msg, self.original_position() - 1));
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
                    return Err(BinaryReaderError::new(
                        "invalid var_s33: integer representation too long",
                        self.original_position() - 1,
                    ));
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
                    let msg = if continuation_bit {
                        "invalid var_i64: integer representation too long"
                    } else {
                        "invalid var_i64: integer too large"
                    };
                    return Err(BinaryReaderError::new(msg, self.original_position() - 1));
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
            return Err(BinaryReaderError::new(
                "string size out of bounds",
                self.original_position() - 1,
            ));
        }
        let bytes = self.read_bytes(len)?;
        str::from_utf8(bytes).map_err(|_| {
            BinaryReaderError::new("invalid UTF-8 encoding", self.original_position() - 1)
        })
    }

    fn read_optional_string(&mut self) -> Result<Option<&'a str>> {
        match self.read_u8()? {
            0x0 => Ok(None),
            0x1 => Ok(Some(self.read_string()?)),
            _ => Err(BinaryReaderError::new(
                "invalid optional string encoding",
                self.original_position() - 1,
            )),
        }
    }

    fn read_memarg_of_align(&mut self, max_align: u8) -> Result<MemoryImmediate> {
        let align_pos = self.original_position();
        let imm = self.read_memarg()?;
        if imm.align > max_align {
            return Err(BinaryReaderError::new(
                "alignment must not be larger than natural",
                align_pos,
            ));
        }
        Ok(imm)
    }

    fn read_0xfe_operator(&mut self) -> Result<Operator<'a>> {
        let code = self.read_var_u32()?;
        Ok(match code {
            0x00 => Operator::MemoryAtomicNotify {
                memarg: self.read_memarg_of_align(2)?,
            },
            0x01 => Operator::MemoryAtomicWait32 {
                memarg: self.read_memarg_of_align(2)?,
            },
            0x02 => Operator::MemoryAtomicWait64 {
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
                return Err(BinaryReaderError::new(
                    format!("unknown 0xfe subopcode: 0x{:x}", code),
                    self.original_position() - 1,
                ));
            }
        })
    }

    #[cold]
    fn invalid_leading_byte<T>(&self, byte: u8, desc: &str) -> Result<T> {
        Err(Self::invalid_leading_byte_error(
            byte,
            desc,
            self.original_position() - 1,
        ))
    }

    #[cold]
    fn invalid_leading_byte_error(byte: u8, desc: &str, offset: usize) -> BinaryReaderError {
        BinaryReaderError::new(
            format!("invalid leading byte (0x{:x}) for {}", byte, desc),
            offset,
        )
    }

    fn peek(&self) -> Result<u8> {
        self.ensure_has_byte()?;
        Ok(self.buffer[self.position])
    }

    fn val_type_from_byte(byte: u8) -> Option<ValType> {
        match byte {
            0x7F => Some(ValType::I32),
            0x7E => Some(ValType::I64),
            0x7D => Some(ValType::F32),
            0x7C => Some(ValType::F64),
            0x7B => Some(ValType::V128),
            0x70 => Some(ValType::FuncRef),
            0x6F => Some(ValType::ExternRef),
            _ => None,
        }
    }

    fn read_block_type(&mut self) -> Result<BlockType> {
        let b = self.peek()?;

        // Check for empty block
        if b == 0x40 {
            self.position += 1;
            return Ok(BlockType::Empty);
        }

        // Check for a block type of form [] -> [t].
        if let Some(ty) = Self::val_type_from_byte(b) {
            self.position += 1;
            return Ok(BlockType::Type(ty));
        }

        // Not empty or a singular type, so read the function type index
        let idx = self.read_var_s33()?;
        if idx < 0 || idx > (std::u32::MAX as i64) {
            return Err(BinaryReaderError::new(
                "invalid function type",
                self.original_position(),
            ));
        }

        Ok(BlockType::FuncType(idx as u32))
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
                ty: self.read_block_type()?,
            },
            0x03 => Operator::Loop {
                ty: self.read_block_type()?,
            },
            0x04 => Operator::If {
                ty: self.read_block_type()?,
            },
            0x05 => Operator::Else,
            0x06 => Operator::Try {
                ty: self.read_block_type()?,
            },
            0x07 => Operator::Catch {
                index: self.read_var_u32()?,
            },
            0x08 => Operator::Throw {
                index: self.read_var_u32()?,
            },
            0x09 => Operator::Rethrow {
                relative_depth: self.read_var_u32()?,
            },
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
            0x11 => {
                let index = self.read_var_u32()?;
                let (table_byte, table_index) = self.read_first_byte_and_var_u32()?;
                Operator::CallIndirect {
                    index,
                    table_index,
                    table_byte,
                }
            }
            0x12 => Operator::ReturnCall {
                function_index: self.read_var_u32()?,
            },
            0x13 => Operator::ReturnCallIndirect {
                index: self.read_var_u32()?,
                table_index: self.read_var_u32()?,
            },
            0x18 => Operator::Delegate {
                relative_depth: self.read_var_u32()?,
            },
            0x19 => Operator::CatchAll,
            0x1a => Operator::Drop,
            0x1b => Operator::Select,
            0x1c => {
                let results = self.read_var_u32()?;
                if results != 1 {
                    return Err(BinaryReaderError::new(
                        "invalid result arity",
                        self.position,
                    ));
                }
                Operator::TypedSelect {
                    ty: self.read_val_type()?,
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
            0x3f => {
                let (mem_byte, mem) = self.read_first_byte_and_var_u32()?;
                Operator::MemorySize { mem_byte, mem }
            }
            0x40 => {
                let (mem_byte, mem) = self.read_first_byte_and_var_u32()?;
                Operator::MemoryGrow { mem_byte, mem }
            }
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

            0xd0 => Operator::RefNull {
                ty: self.read_val_type()?,
            },
            0xd1 => Operator::RefIsNull,
            0xd2 => Operator::RefFunc {
                function_index: self.read_var_u32()?,
            },

            0xfc => self.read_0xfc_operator()?,
            0xfd => self.read_0xfd_operator()?,
            0xfe => self.read_0xfe_operator()?,

            _ => {
                return Err(BinaryReaderError::new(
                    format!("illegal opcode: 0x{:x}", code),
                    self.original_position() - 1,
                ));
            }
        })
    }

    fn read_0xfc_operator(&mut self) -> Result<Operator<'a>> {
        let code = self.read_var_u32()?;
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
                let mem = self.read_var_u32()?;
                Operator::MemoryInit { segment, mem }
            }
            0x09 => {
                let segment = self.read_var_u32()?;
                Operator::DataDrop { segment }
            }
            0x0a => {
                let dst = self.read_var_u32()?;
                let src = self.read_var_u32()?;
                Operator::MemoryCopy { src, dst }
            }
            0x0b => {
                let mem = self.read_var_u32()?;
                Operator::MemoryFill { mem }
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
                return Err(BinaryReaderError::new(
                    format!("unknown 0xfc subopcode: 0x{:x}", code),
                    self.original_position() - 1,
                ));
            }
        })
    }

    fn read_lane_index(&mut self, max: u8) -> Result<SIMDLaneIndex> {
        let index = self.read_u8()?;
        if index >= max {
            return Err(BinaryReaderError::new(
                "invalid lane index",
                self.original_position() - 1,
            ));
        }
        Ok(index as SIMDLaneIndex)
    }

    fn read_v128(&mut self) -> Result<V128> {
        let mut bytes = [0; 16];
        bytes.clone_from_slice(self.read_bytes(16)?);
        Ok(V128(bytes))
    }

    fn read_0xfd_operator(&mut self) -> Result<Operator<'a>> {
        let code = self.read_var_u32()?;
        Ok(match code {
            0x00 => Operator::V128Load {
                memarg: self.read_memarg()?,
            },
            0x01 => Operator::V128Load8x8S {
                memarg: self.read_memarg_of_align(3)?,
            },
            0x02 => Operator::V128Load8x8U {
                memarg: self.read_memarg_of_align(3)?,
            },
            0x03 => Operator::V128Load16x4S {
                memarg: self.read_memarg_of_align(3)?,
            },
            0x04 => Operator::V128Load16x4U {
                memarg: self.read_memarg_of_align(3)?,
            },
            0x05 => Operator::V128Load32x2S {
                memarg: self.read_memarg_of_align(3)?,
            },
            0x06 => Operator::V128Load32x2U {
                memarg: self.read_memarg_of_align(3)?,
            },
            0x07 => Operator::V128Load8Splat {
                memarg: self.read_memarg_of_align(0)?,
            },
            0x08 => Operator::V128Load16Splat {
                memarg: self.read_memarg_of_align(1)?,
            },
            0x09 => Operator::V128Load32Splat {
                memarg: self.read_memarg_of_align(2)?,
            },
            0x0a => Operator::V128Load64Splat {
                memarg: self.read_memarg_of_align(3)?,
            },
            0x0b => Operator::V128Store {
                memarg: self.read_memarg()?,
            },
            0x0c => Operator::V128Const {
                value: self.read_v128()?,
            },
            0x0d => {
                let mut lanes: [SIMDLaneIndex; 16] = [0; 16];
                for lane in &mut lanes {
                    *lane = self.read_lane_index(32)?
                }
                Operator::I8x16Shuffle { lanes }
            }
            0x0e => Operator::I8x16Swizzle,
            0x0f => Operator::I8x16Splat,
            0x10 => Operator::I16x8Splat,
            0x11 => Operator::I32x4Splat,
            0x12 => Operator::I64x2Splat,
            0x13 => Operator::F32x4Splat,
            0x14 => Operator::F64x2Splat,
            0x15 => Operator::I8x16ExtractLaneS {
                lane: self.read_lane_index(16)?,
            },
            0x16 => Operator::I8x16ExtractLaneU {
                lane: self.read_lane_index(16)?,
            },
            0x17 => Operator::I8x16ReplaceLane {
                lane: self.read_lane_index(16)?,
            },
            0x18 => Operator::I16x8ExtractLaneS {
                lane: self.read_lane_index(8)?,
            },
            0x19 => Operator::I16x8ExtractLaneU {
                lane: self.read_lane_index(8)?,
            },
            0x1a => Operator::I16x8ReplaceLane {
                lane: self.read_lane_index(8)?,
            },
            0x1b => Operator::I32x4ExtractLane {
                lane: self.read_lane_index(4)?,
            },
            0x1c => Operator::I32x4ReplaceLane {
                lane: self.read_lane_index(4)?,
            },
            0x1d => Operator::I64x2ExtractLane {
                lane: self.read_lane_index(2)?,
            },
            0x1e => Operator::I64x2ReplaceLane {
                lane: self.read_lane_index(2)?,
            },
            0x1f => Operator::F32x4ExtractLane {
                lane: self.read_lane_index(4)?,
            },
            0x20 => Operator::F32x4ReplaceLane {
                lane: self.read_lane_index(4)?,
            },
            0x21 => Operator::F64x2ExtractLane {
                lane: self.read_lane_index(2)?,
            },
            0x22 => Operator::F64x2ReplaceLane {
                lane: self.read_lane_index(2)?,
            },
            0x23 => Operator::I8x16Eq,
            0x24 => Operator::I8x16Ne,
            0x25 => Operator::I8x16LtS,
            0x26 => Operator::I8x16LtU,
            0x27 => Operator::I8x16GtS,
            0x28 => Operator::I8x16GtU,
            0x29 => Operator::I8x16LeS,
            0x2a => Operator::I8x16LeU,
            0x2b => Operator::I8x16GeS,
            0x2c => Operator::I8x16GeU,
            0x2d => Operator::I16x8Eq,
            0x2e => Operator::I16x8Ne,
            0x2f => Operator::I16x8LtS,
            0x30 => Operator::I16x8LtU,
            0x31 => Operator::I16x8GtS,
            0x32 => Operator::I16x8GtU,
            0x33 => Operator::I16x8LeS,
            0x34 => Operator::I16x8LeU,
            0x35 => Operator::I16x8GeS,
            0x36 => Operator::I16x8GeU,
            0x37 => Operator::I32x4Eq,
            0x38 => Operator::I32x4Ne,
            0x39 => Operator::I32x4LtS,
            0x3a => Operator::I32x4LtU,
            0x3b => Operator::I32x4GtS,
            0x3c => Operator::I32x4GtU,
            0x3d => Operator::I32x4LeS,
            0x3e => Operator::I32x4LeU,
            0x3f => Operator::I32x4GeS,
            0x40 => Operator::I32x4GeU,
            0x41 => Operator::F32x4Eq,
            0x42 => Operator::F32x4Ne,
            0x43 => Operator::F32x4Lt,
            0x44 => Operator::F32x4Gt,
            0x45 => Operator::F32x4Le,
            0x46 => Operator::F32x4Ge,
            0x47 => Operator::F64x2Eq,
            0x48 => Operator::F64x2Ne,
            0x49 => Operator::F64x2Lt,
            0x4a => Operator::F64x2Gt,
            0x4b => Operator::F64x2Le,
            0x4c => Operator::F64x2Ge,
            0x4d => Operator::V128Not,
            0x4e => Operator::V128And,
            0x4f => Operator::V128AndNot,
            0x50 => Operator::V128Or,
            0x51 => Operator::V128Xor,
            0x52 => Operator::V128Bitselect,
            0x53 => Operator::V128AnyTrue,
            0x54 => Operator::V128Load8Lane {
                memarg: self.read_memarg()?,
                lane: self.read_lane_index(16)?,
            },
            0x55 => Operator::V128Load16Lane {
                memarg: self.read_memarg()?,
                lane: self.read_lane_index(8)?,
            },
            0x56 => Operator::V128Load32Lane {
                memarg: self.read_memarg()?,
                lane: self.read_lane_index(4)?,
            },
            0x57 => Operator::V128Load64Lane {
                memarg: self.read_memarg()?,
                lane: self.read_lane_index(2)?,
            },
            0x58 => Operator::V128Store8Lane {
                memarg: self.read_memarg()?,
                lane: self.read_lane_index(16)?,
            },
            0x59 => Operator::V128Store16Lane {
                memarg: self.read_memarg()?,
                lane: self.read_lane_index(8)?,
            },
            0x5a => Operator::V128Store32Lane {
                memarg: self.read_memarg()?,
                lane: self.read_lane_index(4)?,
            },
            0x5b => Operator::V128Store64Lane {
                memarg: self.read_memarg()?,
                lane: self.read_lane_index(2)?,
            },
            0x5c => Operator::V128Load32Zero {
                memarg: self.read_memarg_of_align(2)?,
            },
            0x5d => Operator::V128Load64Zero {
                memarg: self.read_memarg_of_align(3)?,
            },
            0x5e => Operator::F32x4DemoteF64x2Zero,
            0x5f => Operator::F64x2PromoteLowF32x4,
            0x60 => Operator::I8x16Abs,
            0x61 => Operator::I8x16Neg,
            0x62 => Operator::I8x16Popcnt,
            0x63 => Operator::I8x16AllTrue,
            0x64 => Operator::I8x16Bitmask,
            0x65 => Operator::I8x16NarrowI16x8S,
            0x66 => Operator::I8x16NarrowI16x8U,
            0x67 => Operator::F32x4Ceil,
            0x68 => Operator::F32x4Floor,
            0x69 => Operator::F32x4Trunc,
            0x6a => Operator::F32x4Nearest,
            0x6b => Operator::I8x16Shl,
            0x6c => Operator::I8x16ShrS,
            0x6d => Operator::I8x16ShrU,
            0x6e => Operator::I8x16Add,
            0x6f => Operator::I8x16AddSatS,
            0x70 => Operator::I8x16AddSatU,
            0x71 => Operator::I8x16Sub,
            0x72 => Operator::I8x16SubSatS,
            0x73 => Operator::I8x16SubSatU,
            0x74 => Operator::F64x2Ceil,
            0x75 => Operator::F64x2Floor,
            0x76 => Operator::I8x16MinS,
            0x77 => Operator::I8x16MinU,
            0x78 => Operator::I8x16MaxS,
            0x79 => Operator::I8x16MaxU,
            0x7a => Operator::F64x2Trunc,
            0x7b => Operator::I8x16RoundingAverageU,
            0x7c => Operator::I16x8ExtAddPairwiseI8x16S,
            0x7d => Operator::I16x8ExtAddPairwiseI8x16U,
            0x7e => Operator::I32x4ExtAddPairwiseI16x8S,
            0x7f => Operator::I32x4ExtAddPairwiseI16x8U,
            0x80 => Operator::I16x8Abs,
            0x81 => Operator::I16x8Neg,
            0x82 => Operator::I16x8Q15MulrSatS,
            0x83 => Operator::I16x8AllTrue,
            0x84 => Operator::I16x8Bitmask,
            0x85 => Operator::I16x8NarrowI32x4S,
            0x86 => Operator::I16x8NarrowI32x4U,
            0x87 => Operator::I16x8ExtendLowI8x16S,
            0x88 => Operator::I16x8ExtendHighI8x16S,
            0x89 => Operator::I16x8ExtendLowI8x16U,
            0x8a => Operator::I16x8ExtendHighI8x16U,
            0x8b => Operator::I16x8Shl,
            0x8c => Operator::I16x8ShrS,
            0x8d => Operator::I16x8ShrU,
            0x8e => Operator::I16x8Add,
            0x8f => Operator::I16x8AddSatS,
            0x90 => Operator::I16x8AddSatU,
            0x91 => Operator::I16x8Sub,
            0x92 => Operator::I16x8SubSatS,
            0x93 => Operator::I16x8SubSatU,
            0x94 => Operator::F64x2Nearest,
            0x95 => Operator::I16x8Mul,
            0x96 => Operator::I16x8MinS,
            0x97 => Operator::I16x8MinU,
            0x98 => Operator::I16x8MaxS,
            0x99 => Operator::I16x8MaxU,
            0x9b => Operator::I16x8RoundingAverageU,
            0x9c => Operator::I16x8ExtMulLowI8x16S,
            0x9d => Operator::I16x8ExtMulHighI8x16S,
            0x9e => Operator::I16x8ExtMulLowI8x16U,
            0x9f => Operator::I16x8ExtMulHighI8x16U,
            0xa0 => Operator::I32x4Abs,
            0xa2 => Operator::I8x16RelaxedSwizzle,
            0xa1 => Operator::I32x4Neg,
            0xa3 => Operator::I32x4AllTrue,
            0xa4 => Operator::I32x4Bitmask,
            0xa5 => Operator::I32x4RelaxedTruncSatF32x4S,
            0xa6 => Operator::I32x4RelaxedTruncSatF32x4U,
            0xa7 => Operator::I32x4ExtendLowI16x8S,
            0xa8 => Operator::I32x4ExtendHighI16x8S,
            0xa9 => Operator::I32x4ExtendLowI16x8U,
            0xaa => Operator::I32x4ExtendHighI16x8U,
            0xab => Operator::I32x4Shl,
            0xac => Operator::I32x4ShrS,
            0xad => Operator::I32x4ShrU,
            0xae => Operator::I32x4Add,
            0xaf => Operator::F32x4Fma,
            0xb0 => Operator::F32x4Fms,
            0xb1 => Operator::I32x4Sub,
            0xb2 => Operator::I8x16LaneSelect,
            0xb3 => Operator::I16x8LaneSelect,
            0xb4 => Operator::F32x4RelaxedMin,
            0xb5 => Operator::I32x4Mul,
            0xb6 => Operator::I32x4MinS,
            0xb7 => Operator::I32x4MinU,
            0xb8 => Operator::I32x4MaxS,
            0xb9 => Operator::I32x4MaxU,
            0xba => Operator::I32x4DotI16x8S,
            0xbc => Operator::I32x4ExtMulLowI16x8S,
            0xbd => Operator::I32x4ExtMulHighI16x8S,
            0xbe => Operator::I32x4ExtMulLowI16x8U,
            0xbf => Operator::I32x4ExtMulHighI16x8U,
            0xc0 => Operator::I64x2Abs,
            0xc1 => Operator::I64x2Neg,
            0xc3 => Operator::I64x2AllTrue,
            0xc4 => Operator::I64x2Bitmask,
            0xc5 => Operator::I32x4RelaxedTruncSatF64x2SZero,
            0xc6 => Operator::I32x4RelaxedTruncSatF64x2UZero,
            0xc7 => Operator::I64x2ExtendLowI32x4S,
            0xc8 => Operator::I64x2ExtendHighI32x4S,
            0xc9 => Operator::I64x2ExtendLowI32x4U,
            0xca => Operator::I64x2ExtendHighI32x4U,
            0xcb => Operator::I64x2Shl,
            0xcc => Operator::I64x2ShrS,
            0xcd => Operator::I64x2ShrU,
            0xce => Operator::I64x2Add,
            0xcf => Operator::F64x2Fma,
            0xd0 => Operator::F64x2Fms,
            0xd1 => Operator::I64x2Sub,
            0xd2 => Operator::I32x4LaneSelect,
            0xd3 => Operator::I64x2LaneSelect,
            0xd4 => Operator::F64x2RelaxedMin,
            0xd5 => Operator::I64x2Mul,
            0xd6 => Operator::I64x2Eq,
            0xd7 => Operator::I64x2Ne,
            0xd8 => Operator::I64x2LtS,
            0xd9 => Operator::I64x2GtS,
            0xda => Operator::I64x2LeS,
            0xdb => Operator::I64x2GeS,
            0xdc => Operator::I64x2ExtMulLowI32x4S,
            0xdd => Operator::I64x2ExtMulHighI32x4S,
            0xde => Operator::I64x2ExtMulLowI32x4U,
            0xdf => Operator::I64x2ExtMulHighI32x4U,
            0xe0 => Operator::F32x4Abs,
            0xe1 => Operator::F32x4Neg,
            0xe2 => Operator::F32x4RelaxedMax,
            0xe3 => Operator::F32x4Sqrt,
            0xe4 => Operator::F32x4Add,
            0xe5 => Operator::F32x4Sub,
            0xe6 => Operator::F32x4Mul,
            0xe7 => Operator::F32x4Div,
            0xe8 => Operator::F32x4Min,
            0xe9 => Operator::F32x4Max,
            0xea => Operator::F32x4PMin,
            0xeb => Operator::F32x4PMax,
            0xec => Operator::F64x2Abs,
            0xed => Operator::F64x2Neg,
            0xee => Operator::F64x2RelaxedMax,
            0xef => Operator::F64x2Sqrt,
            0xf0 => Operator::F64x2Add,
            0xf1 => Operator::F64x2Sub,
            0xf2 => Operator::F64x2Mul,
            0xf3 => Operator::F64x2Div,
            0xf4 => Operator::F64x2Min,
            0xf5 => Operator::F64x2Max,
            0xf6 => Operator::F64x2PMin,
            0xf7 => Operator::F64x2PMax,
            0xf8 => Operator::I32x4TruncSatF32x4S,
            0xf9 => Operator::I32x4TruncSatF32x4U,
            0xfa => Operator::F32x4ConvertI32x4S,
            0xfb => Operator::F32x4ConvertI32x4U,
            0xfc => Operator::I32x4TruncSatF64x2SZero,
            0xfd => Operator::I32x4TruncSatF64x2UZero,
            0xfe => Operator::F64x2ConvertLowI32x4S,
            0xff => Operator::F64x2ConvertLowI32x4U,

            _ => {
                return Err(BinaryReaderError::new(
                    format!("unknown 0xfd subopcode: 0x{:x}", code),
                    self.original_position() - 1,
                ));
            }
        })
    }

    pub(crate) fn read_header_version(&mut self) -> Result<u32> {
        let magic_number = self.read_bytes(4)?;
        if magic_number != WASM_MAGIC_NUMBER {
            return Err(BinaryReaderError::new(
                "magic header not detected: bad magic number",
                self.original_position() - 4,
            ));
        }
        self.read_u32()
    }

    pub(crate) fn read_name_type(&mut self) -> Result<NameType> {
        let code = self.read_u7()?;
        match code {
            0 => Ok(NameType::Module),
            1 => Ok(NameType::Function),
            2 => Ok(NameType::Local),
            3 => Ok(NameType::Label),
            4 => Ok(NameType::Type),
            5 => Ok(NameType::Table),
            6 => Ok(NameType::Memory),
            7 => Ok(NameType::Global),
            8 => Ok(NameType::Element),
            9 => Ok(NameType::Data),
            _ => Ok(NameType::Unknown(code)),
        }
    }

    pub(crate) fn read_linking_type(&mut self) -> Result<LinkingType> {
        let ty = self.read_var_u32()?;
        Ok(match ty {
            1 => LinkingType::StackPointer(self.read_var_u32()?),
            _ => {
                return Err(BinaryReaderError::new(
                    "invalid linking type",
                    self.original_position() - 1,
                ));
            }
        })
    }

    pub(crate) fn read_reloc_type(&mut self) -> Result<RelocType> {
        let code = self.read_u7()?;
        match code {
            0 => Ok(RelocType::FunctionIndexLEB),
            1 => Ok(RelocType::TableIndexSLEB),
            2 => Ok(RelocType::TableIndexI32),
            3 => Ok(RelocType::GlobalAddrLEB),
            4 => Ok(RelocType::GlobalAddrSLEB),
            5 => Ok(RelocType::GlobalAddrI32),
            6 => Ok(RelocType::TypeIndexLEB),
            7 => Ok(RelocType::GlobalIndexLEB),
            _ => Err(BinaryReaderError::new(
                "invalid reloc type",
                self.original_position() - 1,
            )),
        }
    }

    pub(crate) fn read_init_expr(&mut self) -> Result<InitExpr<'a>> {
        let expr_offset = self.position;
        self.skip_init_expr()?;
        let data = &self.buffer[expr_offset..self.position];
        Ok(InitExpr::new(data, self.original_offset + expr_offset))
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
    pub fn len(&self) -> u32 {
        self.cnt
    }

    /// Returns whether `BrTable` doesn't have any labels apart from the default one.
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Returns the default target of this `br_table` instruction.
    pub fn default(&self) -> u32 {
        self.default
    }

    /// Returns the list of targets that this `br_table` instruction will be
    /// jumping to.
    ///
    /// This method will return an iterator which parses each target of this
    /// `br_table` except the default target. The returned iterator will
    /// yield `self.len()` elements.
    ///
    /// # Examples
    ///
    /// ```rust
    /// let buf = [0x0e, 0x02, 0x01, 0x02, 0x00];
    /// let mut reader = wasmparser::BinaryReader::new(&buf);
    /// let op = reader.read_operator().unwrap();
    /// if let wasmparser::Operator::BrTable { table } = op {
    ///     let targets = table.targets().collect::<Result<Vec<_>, _>>().unwrap();
    ///     assert_eq!(targets, [1, 2]);
    /// }
    /// ```
    pub fn targets(&self) -> BrTableTargets {
        BrTableTargets {
            reader: self.reader.clone(),
            remaining: self.cnt,
        }
    }
}

/// An iterator over the targets of a [`BrTable`].
///
/// # Note
///
/// This iterator parses each target of the underlying `br_table`
/// except for the default target.
/// The iterator will yield exactly as many targets as the `br_table` has.
pub struct BrTableTargets<'a> {
    reader: crate::BinaryReader<'a>,
    remaining: u32,
}

impl<'a> Iterator for BrTableTargets<'a> {
    type Item = Result<u32>;

    fn size_hint(&self) -> (usize, Option<usize>) {
        let remaining = usize::try_from(self.remaining).unwrap_or_else(|error| {
            panic!("could not convert remaining `u32` into `usize`: {}", error)
        });
        (remaining, Some(remaining))
    }

    fn next(&mut self) -> Option<Self::Item> {
        if self.remaining == 0 {
            if !self.reader.eof() {
                return Some(Err(BinaryReaderError::new(
                    "trailing data in br_table",
                    self.reader.original_position(),
                )));
            }
            return None;
        }
        self.remaining -= 1;
        Some(self.reader.read_var_u32())
    }
}

impl fmt::Debug for BrTable<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let mut f = f.debug_struct("BrTable");
        f.field("count", &self.cnt);
        f.field("default", &self.default);
        match self.targets().collect::<Result<Vec<_>>>() {
            Ok(targets) => {
                f.field("targets", &targets);
            }
            Err(_) => {
                f.field("reader", &self.reader);
            }
        }
        f.finish()
    }
}
