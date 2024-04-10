/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::metadata::{checksum_metadata, codes};
use crate::*;
use anyhow::{bail, ensure, Context, Result};

pub fn read_metadata(data: &[u8]) -> Result<Metadata> {
    MetadataReader::new(data).read_metadata()
}

// Read a metadata type, this is pub so that we can test it in the metadata fixture
pub fn read_metadata_type(data: &[u8]) -> Result<Type> {
    MetadataReader::new(data).read_type()
}

/// Helper struct for read_metadata()
struct MetadataReader<'a> {
    // This points to the initial data we were passed in
    initial_data: &'a [u8],
    // This points to the remaining data to be read
    buf: &'a [u8],
}

impl<'a> MetadataReader<'a> {
    fn new(data: &'a [u8]) -> Self {
        Self {
            initial_data: data,
            buf: data,
        }
    }

    // Read a top-level metadata item
    //
    // This consumes self because MetadataReader is only intended to read a single item.
    fn read_metadata(mut self) -> Result<Metadata> {
        let value = self.read_u8()?;
        Ok(match value {
            codes::NAMESPACE => NamespaceMetadata {
                crate_name: self.read_string()?,
                name: self.read_string()?,
            }
            .into(),
            codes::UDL_FILE => UdlFile {
                module_path: self.read_string()?,
                namespace: self.read_string()?,
                file_stub: self.read_string()?,
            }
            .into(),
            codes::FUNC => self.read_func()?.into(),
            codes::CONSTRUCTOR => self.read_constructor()?.into(),
            codes::METHOD => self.read_method()?.into(),
            codes::RECORD => self.read_record()?.into(),
            codes::ENUM => self.read_enum(false)?.into(),
            codes::ERROR => self.read_error()?.into(),
            codes::INTERFACE => self.read_object()?.into(),
            codes::CALLBACK_INTERFACE => self.read_callback_interface()?.into(),
            codes::TRAIT_METHOD => self.read_trait_method()?.into(),
            codes::UNIFFI_TRAIT => self.read_uniffi_trait()?.into(),
            _ => bail!("Unexpected metadata code: {value:?}"),
        })
    }

    fn read_u8(&mut self) -> Result<u8> {
        if !self.buf.is_empty() {
            let value = self.buf[0];
            self.buf = &self.buf[1..];
            Ok(value)
        } else {
            bail!("Buffer is empty")
        }
    }

    fn peek_u8(&mut self) -> Result<u8> {
        if !self.buf.is_empty() {
            Ok(self.buf[0])
        } else {
            bail!("Buffer is empty")
        }
    }

    fn read_u32(&mut self) -> Result<u32> {
        if self.buf.len() >= 4 {
            // read the value as little-endian
            let value = self.buf[0] as u32
                + ((self.buf[1] as u32) << 8)
                + ((self.buf[2] as u32) << 16)
                + ((self.buf[3] as u32) << 24);
            self.buf = &self.buf[4..];
            Ok(value)
        } else {
            bail!("Not enough data left in buffer to read a u32 value");
        }
    }

    fn read_bool(&mut self) -> Result<bool> {
        Ok(self.read_u8()? == 1)
    }

    fn read_string(&mut self) -> Result<String> {
        let size = self.read_u8()? as usize;
        let slice;
        (slice, self.buf) = self.buf.split_at(size);
        String::from_utf8(slice.into()).context("Invalid string data")
    }

    fn read_type(&mut self) -> Result<Type> {
        let value = self.read_u8()?;
        Ok(match value {
            codes::TYPE_U8 => Type::UInt8,
            codes::TYPE_I8 => Type::Int8,
            codes::TYPE_U16 => Type::UInt16,
            codes::TYPE_I16 => Type::Int16,
            codes::TYPE_U32 => Type::UInt32,
            codes::TYPE_I32 => Type::Int32,
            codes::TYPE_U64 => Type::UInt64,
            codes::TYPE_I64 => Type::Int64,
            codes::TYPE_F32 => Type::Float32,
            codes::TYPE_F64 => Type::Float64,
            codes::TYPE_BOOL => Type::Boolean,
            codes::TYPE_STRING => Type::String,
            codes::TYPE_DURATION => Type::Duration,
            codes::TYPE_SYSTEM_TIME => Type::Timestamp,
            codes::TYPE_FOREIGN_EXECUTOR => Type::ForeignExecutor,
            codes::TYPE_RECORD => Type::Record {
                module_path: self.read_string()?,
                name: self.read_string()?,
            },
            codes::TYPE_ENUM => Type::Enum {
                module_path: self.read_string()?,
                name: self.read_string()?,
            },
            codes::TYPE_INTERFACE => Type::Object {
                module_path: self.read_string()?,
                name: self.read_string()?,
                imp: ObjectImpl::from_is_trait(self.read_bool()?),
            },
            codes::TYPE_CALLBACK_INTERFACE => Type::CallbackInterface {
                module_path: self.read_string()?,
                name: self.read_string()?,
            },
            codes::TYPE_CUSTOM => Type::Custom {
                module_path: self.read_string()?,
                name: self.read_string()?,
                builtin: Box::new(self.read_type()?),
            },
            codes::TYPE_OPTION => Type::Optional {
                inner_type: Box::new(self.read_type()?),
            },
            codes::TYPE_VEC => {
                let inner_type = self.read_type()?;
                if inner_type == Type::UInt8 {
                    Type::Bytes
                } else {
                    Type::Sequence {
                        inner_type: Box::new(inner_type),
                    }
                }
            }
            codes::TYPE_HASH_MAP => Type::Map {
                key_type: Box::new(self.read_type()?),
                value_type: Box::new(self.read_type()?),
            },
            codes::TYPE_UNIT => bail!("Unexpected TYPE_UNIT"),
            codes::TYPE_RESULT => bail!("Unexpected TYPE_RESULT"),
            _ => bail!("Unexpected metadata type code: {value:?}"),
        })
    }

    fn read_optional_type(&mut self) -> Result<Option<Type>> {
        Ok(match self.peek_u8()? {
            codes::TYPE_UNIT => {
                _ = self.read_u8();
                None
            }
            _ => Some(self.read_type()?),
        })
    }

    fn read_return_type(&mut self) -> Result<(Option<Type>, Option<Type>)> {
        Ok(match self.peek_u8()? {
            codes::TYPE_UNIT => {
                _ = self.read_u8();
                (None, None)
            }
            codes::TYPE_RESULT => {
                _ = self.read_u8();
                (self.read_optional_type()?, self.read_optional_type()?)
            }
            _ => (Some(self.read_type()?), None),
        })
    }

    fn read_func(&mut self) -> Result<FnMetadata> {
        let module_path = self.read_string()?;
        let name = self.read_string()?;
        let is_async = self.read_bool()?;
        let inputs = self.read_inputs()?;
        let (return_type, throws) = self.read_return_type()?;
        Ok(FnMetadata {
            module_path,
            name,
            is_async,
            inputs,
            return_type,
            throws,
            checksum: self.calc_checksum(),
        })
    }

    fn read_constructor(&mut self) -> Result<ConstructorMetadata> {
        let module_path = self.read_string()?;
        let self_name = self.read_string()?;
        let name = self.read_string()?;
        let inputs = self.read_inputs()?;
        let (return_type, throws) = self.read_return_type()?;

        return_type
            .filter(|t| {
                matches!(
                    t,
                    Type::Object { name, imp: ObjectImpl::Struct, .. } if name == &self_name
                )
            })
            .context("Constructor return type must be Arc<Self>")?;

        Ok(ConstructorMetadata {
            module_path,
            self_name,
            name,
            inputs,
            throws,
            checksum: self.calc_checksum(),
        })
    }

    fn read_method(&mut self) -> Result<MethodMetadata> {
        let module_path = self.read_string()?;
        let self_name = self.read_string()?;
        let name = self.read_string()?;
        let is_async = self.read_bool()?;
        let inputs = self.read_inputs()?;
        let (return_type, throws) = self.read_return_type()?;
        Ok(MethodMetadata {
            module_path,
            self_name,
            name,
            is_async,
            inputs,
            return_type,
            throws,
            takes_self_by_arc: false, // not emitted by macros
            checksum: self.calc_checksum(),
        })
    }

    fn read_record(&mut self) -> Result<RecordMetadata> {
        Ok(RecordMetadata {
            module_path: self.read_string()?,
            name: self.read_string()?,
            fields: self.read_fields()?,
        })
    }

    fn read_enum(&mut self, is_flat_error: bool) -> Result<EnumMetadata> {
        let module_path = self.read_string()?;
        let name = self.read_string()?;
        let variants = if is_flat_error {
            self.read_flat_variants()?
        } else {
            self.read_variants()?
        };

        Ok(EnumMetadata {
            module_path,
            name,
            variants,
        })
    }

    fn read_error(&mut self) -> Result<ErrorMetadata> {
        let is_flat = self.read_bool()?;
        let enum_ = self.read_enum(is_flat)?;
        Ok(ErrorMetadata::Enum { enum_, is_flat })
    }

    fn read_object(&mut self) -> Result<ObjectMetadata> {
        Ok(ObjectMetadata {
            module_path: self.read_string()?,
            name: self.read_string()?,
            imp: ObjectImpl::from_is_trait(self.read_bool()?),
        })
    }

    fn read_uniffi_trait(&mut self) -> Result<UniffiTraitMetadata> {
        let code = self.read_u8()?;
        let mut read_metadata_method = || -> Result<MethodMetadata> {
            let code = self.read_u8()?;
            ensure!(code == codes::METHOD, "expected METHOD but read {code}");
            self.read_method()
        };

        Ok(match UniffiTraitDiscriminants::from(code)? {
            UniffiTraitDiscriminants::Debug => UniffiTraitMetadata::Debug {
                fmt: read_metadata_method()?,
            },
            UniffiTraitDiscriminants::Display => UniffiTraitMetadata::Display {
                fmt: read_metadata_method()?,
            },
            UniffiTraitDiscriminants::Eq => UniffiTraitMetadata::Eq {
                eq: read_metadata_method()?,
                ne: read_metadata_method()?,
            },
            UniffiTraitDiscriminants::Hash => UniffiTraitMetadata::Hash {
                hash: read_metadata_method()?,
            },
        })
    }

    fn read_callback_interface(&mut self) -> Result<CallbackInterfaceMetadata> {
        Ok(CallbackInterfaceMetadata {
            module_path: self.read_string()?,
            name: self.read_string()?,
        })
    }

    fn read_trait_method(&mut self) -> Result<TraitMethodMetadata> {
        let module_path = self.read_string()?;
        let trait_name = self.read_string()?;
        let index = self.read_u32()?;
        let name = self.read_string()?;
        let is_async = self.read_bool()?;
        let inputs = self.read_inputs()?;
        let (return_type, throws) = self.read_return_type()?;
        Ok(TraitMethodMetadata {
            module_path,
            trait_name,
            index,
            name,
            is_async,
            inputs,
            return_type,
            throws,
            takes_self_by_arc: false, // not emitted by macros
            checksum: self.calc_checksum(),
        })
    }

    fn read_fields(&mut self) -> Result<Vec<FieldMetadata>> {
        let len = self.read_u8()?;
        (0..len)
            .map(|_| {
                let name = self.read_string()?;
                let ty = self.read_type()?;
                let default = self.read_default(&name, &ty)?;
                Ok(FieldMetadata { name, ty, default })
            })
            .collect()
    }

    fn read_variants(&mut self) -> Result<Vec<VariantMetadata>> {
        let len = self.read_u8()?;
        (0..len)
            .map(|_| {
                Ok(VariantMetadata {
                    name: self.read_string()?,
                    fields: self.read_fields()?,
                })
            })
            .collect()
    }

    fn read_flat_variants(&mut self) -> Result<Vec<VariantMetadata>> {
        let len = self.read_u8()?;
        (0..len)
            .map(|_| {
                Ok(VariantMetadata {
                    name: self.read_string()?,
                    fields: vec![],
                })
            })
            .collect()
    }

    fn read_inputs(&mut self) -> Result<Vec<FnParamMetadata>> {
        let len = self.read_u8()?;
        (0..len)
            .map(|_| {
                Ok(FnParamMetadata {
                    name: self.read_string()?,
                    ty: self.read_type()?,
                    // not emitted by macros
                    by_ref: false,
                    optional: false,
                    default: None,
                })
            })
            .collect()
    }

    fn calc_checksum(&self) -> Option<u16> {
        let bytes_read = self.initial_data.len() - self.buf.len();
        let metadata_buf = &self.initial_data[..bytes_read];
        Some(checksum_metadata(metadata_buf))
    }

    fn read_default(&mut self, name: &str, ty: &Type) -> Result<Option<LiteralMetadata>> {
        let has_default = self.read_bool()?;
        if !has_default {
            return Ok(None);
        }

        let literal_kind = self.read_u8()?;
        Ok(Some(match literal_kind {
            codes::LIT_STR => {
                ensure!(
                    matches!(ty, Type::String),
                    "field {name} of type {ty:?} can't have a default value of type string"
                );
                LiteralMetadata::String(self.read_string()?)
            }
            codes::LIT_INT => {
                let base10_digits = self.read_string()?;
                macro_rules! parse_int {
                    ($ty:ident, $variant:ident) => {
                        LiteralMetadata::$variant(
                            base10_digits
                                .parse::<$ty>()
                                .with_context(|| format!("parsing default for field {name}"))?
                                .into(),
                            Radix::Decimal,
                            ty.to_owned(),
                        )
                    };
                }

                match ty {
                    Type::UInt8 => parse_int!(u8, UInt),
                    Type::Int8 => parse_int!(i8, Int),
                    Type::UInt16 => parse_int!(u16, UInt),
                    Type::Int16 => parse_int!(i16, Int),
                    Type::UInt32 => parse_int!(u32, UInt),
                    Type::Int32 => parse_int!(i32, Int),
                    Type::UInt64 => parse_int!(u64, UInt),
                    Type::Int64 => parse_int!(i64, Int),
                    _ => {
                        bail!("field {name} of type {ty:?} can't have a default value of type integer");
                    }
                }
            }
            codes::LIT_FLOAT => match ty {
                Type::Float32 | Type::Float64 => {
                    LiteralMetadata::Float(self.read_string()?, ty.to_owned())
                }
                _ => {
                    bail!("field {name} of type {ty:?} can't have a default value of type float");
                }
            },
            codes::LIT_BOOL => LiteralMetadata::Boolean(self.read_bool()?),
            codes::LIT_NULL => LiteralMetadata::Null,
            _ => bail!("Unexpected literal kind code: {literal_kind:?}"),
        }))
    }
}
