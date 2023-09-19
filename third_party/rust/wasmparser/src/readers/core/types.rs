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

use std::fmt::{self, Debug, Write};
use std::slice;

use crate::limits::{
    MAX_WASM_FUNCTION_PARAMS, MAX_WASM_FUNCTION_RETURNS, MAX_WASM_STRUCT_FIELDS,
    MAX_WASM_SUPERTYPES, MAX_WASM_TYPES,
};
use crate::{BinaryReader, BinaryReaderError, FromReader, Result, SectionLimited};

/// Represents the types of values in a WebAssembly module.
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum ValType {
    /// The value type is i32.
    I32,
    /// The value type is i64.
    I64,
    /// The value type is f32.
    F32,
    /// The value type is f64.
    F64,
    /// The value type is v128.
    V128,
    /// The value type is a reference.
    Ref(RefType),
}

/// Represents storage types introduced in the GC spec for array and struct fields.
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum StorageType {
    /// The storage type is i8.
    I8,
    /// The storage type is i16.
    I16,
    /// The storage type is a value type.
    Val(ValType),
}

// The size of `ValType` is performance sensitive.
const _: () = {
    assert!(std::mem::size_of::<ValType>() == 4);
};

pub(crate) trait Inherits {
    fn inherits<'a, F>(
        &self,
        other: &Self,
        self_base_idx: Option<u32>,
        other_base_idx: Option<u32>,
        type_at: &F,
    ) -> bool
    where
        F: Fn(u32) -> &'a SubType;
}

impl From<RefType> for ValType {
    fn from(ty: RefType) -> ValType {
        ValType::Ref(ty)
    }
}

impl ValType {
    /// Alias for the wasm `funcref` type.
    pub const FUNCREF: ValType = ValType::Ref(RefType::FUNCREF);

    /// Alias for the wasm `externref` type.
    pub const EXTERNREF: ValType = ValType::Ref(RefType::EXTERNREF);

    /// Returns whether this value type is a "reference type".
    ///
    /// Only reference types are allowed in tables, for example, and with some
    /// instructions. Current reference types include `funcref` and `externref`.
    pub fn is_reference_type(&self) -> bool {
        matches!(self, ValType::Ref(_))
    }

    /// Whether the type is defaultable, i.e. it is not a non-nullable reference
    /// type.
    pub fn is_defaultable(&self) -> bool {
        match *self {
            Self::I32 | Self::I64 | Self::F32 | Self::F64 | Self::V128 => true,
            Self::Ref(rt) => rt.is_nullable(),
        }
    }

    pub(crate) fn is_valtype_byte(byte: u8) -> bool {
        match byte {
            0x7F | 0x7E | 0x7D | 0x7C | 0x7B | 0x70 | 0x6F | 0x64 | 0x63 | 0x6E | 0x71 | 0x72
            | 0x73 | 0x6D | 0x6B | 0x6A | 0x6C => true,
            _ => false,
        }
    }
}

impl Inherits for ValType {
    fn inherits<'a, F>(
        &self,
        other: &Self,
        self_base_idx: Option<u32>,
        other_base_idx: Option<u32>,
        type_at: &F,
    ) -> bool
    where
        F: Fn(u32) -> &'a SubType,
    {
        match (self, other) {
            (Self::Ref(r1), Self::Ref(r2)) => {
                r1.inherits(r2, self_base_idx, other_base_idx, type_at)
            }
            (
                s @ (Self::I32 | Self::I64 | Self::F32 | Self::F64 | Self::V128 | Self::Ref(_)),
                o,
            ) => s == o,
        }
    }
}

impl<'a> FromReader<'a> for StorageType {
    fn from_reader(reader: &mut BinaryReader<'a>) -> Result<Self> {
        match reader.peek()? {
            0x78 => {
                reader.position += 1;
                Ok(StorageType::I8)
            }
            0x77 => {
                reader.position += 1;
                Ok(StorageType::I16)
            }
            _ => Ok(StorageType::Val(reader.read()?)),
        }
    }
}

impl<'a> FromReader<'a> for ValType {
    fn from_reader(reader: &mut BinaryReader<'a>) -> Result<Self> {
        match reader.peek()? {
            0x7F => {
                reader.position += 1;
                Ok(ValType::I32)
            }
            0x7E => {
                reader.position += 1;
                Ok(ValType::I64)
            }
            0x7D => {
                reader.position += 1;
                Ok(ValType::F32)
            }
            0x7C => {
                reader.position += 1;
                Ok(ValType::F64)
            }
            0x7B => {
                reader.position += 1;
                Ok(ValType::V128)
            }
            0x70 | 0x6F | 0x64 | 0x63 | 0x6E | 0x71 | 0x72 | 0x73 | 0x6D | 0x6B | 0x6A | 0x6C => {
                Ok(ValType::Ref(reader.read()?))
            }
            _ => bail!(reader.original_position(), "invalid value type"),
        }
    }
}

impl fmt::Display for ValType {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let s = match self {
            ValType::I32 => "i32",
            ValType::I64 => "i64",
            ValType::F32 => "f32",
            ValType::F64 => "f64",
            ValType::V128 => "v128",
            ValType::Ref(r) => return fmt::Display::fmt(r, f),
        };
        f.write_str(s)
    }
}

/// A reference type.
///
/// The reference types proposal first introduced `externref` and `funcref`.
///
/// The function references proposal introduced typed function references.
///
/// The GC proposal introduces heap types: any, eq, i31, struct, array, nofunc, noextern, none.
//
// RefType is a bit-packed enum that fits in a `u24` aka `[u8; 3]`.
// Note that its content is opaque (and subject to change), but its API is stable.
// It has the following internal structure:
// ```
// [nullable:u1] [indexed==1:u1] [kind:u2] [index:u20]
// [nullable:u1] [indexed==0:u1] [type:u4] [(unused):u18]
// ```
// , where
// - `nullable` determines nullability of the ref
// - `indexed` determines if the ref is of a dynamically defined type with an index (encoded in a following bit-packing section) or of a known fixed type
// - `kind` determines what kind of indexed type the index is pointing to:
//   ```
//   10 = struct
//   11 = array
//   01 = function
//   ```
// - `index` is the type index
// - `type` is an enumeration of known types:
//   ```
//   1111 = any
//
//   1101 = eq
//   1000 = i31
//   1001 = struct
//   1100 = array
//
//   0101 = func
//   0100 = nofunc
//
//   0011 = extern
//   0010 = noextern
//
//   0000 = none
//   ```
// - `(unused)` is unused sequence of bits
#[derive(Copy, Clone, PartialEq, Eq, Hash)]
pub struct RefType([u8; 3]);

impl Debug for RefType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match (self.is_nullable(), self.heap_type()) {
            (true, HeapType::Any) => write!(f, "anyref"),
            (false, HeapType::Any) => write!(f, "(ref any)"),
            (true, HeapType::None) => write!(f, "nullref"),
            (false, HeapType::None) => write!(f, "(ref none)"),
            (true, HeapType::NoExtern) => write!(f, "nullexternref"),
            (false, HeapType::NoExtern) => write!(f, "(ref noextern)"),
            (true, HeapType::NoFunc) => write!(f, "nullfuncref"),
            (false, HeapType::NoFunc) => write!(f, "(ref nofunc)"),
            (true, HeapType::Eq) => write!(f, "eqref"),
            (false, HeapType::Eq) => write!(f, "(ref eq)"),
            (true, HeapType::Struct) => write!(f, "structref"),
            (false, HeapType::Struct) => write!(f, "(ref struct)"),
            (true, HeapType::Array) => write!(f, "arrayref"),
            (false, HeapType::Array) => write!(f, "(ref array)"),
            (true, HeapType::I31) => write!(f, "i31ref"),
            (false, HeapType::I31) => write!(f, "(ref i31)"),
            (true, HeapType::Extern) => write!(f, "externref"),
            (false, HeapType::Extern) => write!(f, "(ref extern)"),
            (true, HeapType::Func) => write!(f, "funcref"),
            (false, HeapType::Func) => write!(f, "(ref func)"),
            (true, HeapType::Indexed(idx)) => write!(f, "(ref null {idx})"),
            (false, HeapType::Indexed(idx)) => write!(f, "(ref {idx})"),
        }
    }
}

// Static assert that we can fit indices up to `MAX_WASM_TYPES` inside `RefType`.
const _: () = {
    const fn can_roundtrip_index(index: u32) -> bool {
        assert!(RefType::can_represent_type_index(index));
        let rt = match RefType::indexed_func(true, index) {
            Some(rt) => rt,
            None => panic!(),
        };
        assert!(rt.is_nullable());
        let actual_index = match rt.type_index() {
            Some(i) => i,
            None => panic!(),
        };
        actual_index == index
    }

    assert!(can_roundtrip_index(crate::limits::MAX_WASM_TYPES as u32));
    assert!(can_roundtrip_index(0b00000000_00001111_00000000_00000000));
    assert!(can_roundtrip_index(0b00000000_00000000_11111111_00000000));
    assert!(can_roundtrip_index(0b00000000_00000000_00000000_11111111));
    assert!(can_roundtrip_index(0));
};

impl RefType {
    const NULLABLE_BIT: u32 = 1 << 23; // bit #23
    const INDEXED_BIT: u32 = 1 << 22; // bit #22

    const TYPE_MASK: u32 = 0b1111 << 18; // 4 bits #21-#18 (if `indexed == 0`)
    const ANY_TYPE: u32 = 0b1111 << 18;
    const EQ_TYPE: u32 = 0b1101 << 18;
    const I31_TYPE: u32 = 0b1000 << 18;
    const STRUCT_TYPE: u32 = 0b1001 << 18;
    const ARRAY_TYPE: u32 = 0b1100 << 18;
    const FUNC_TYPE: u32 = 0b0101 << 18;
    const NOFUNC_TYPE: u32 = 0b0100 << 18;
    const EXTERN_TYPE: u32 = 0b0011 << 18;
    const NOEXTERN_TYPE: u32 = 0b0010 << 18;
    const NONE_TYPE: u32 = 0b0000 << 18;

    const KIND_MASK: u32 = 0b11 << 20; // 2 bits #21-#20 (if `indexed == 1`)
    const STRUCT_KIND: u32 = 0b10 << 20;
    const ARRAY_KIND: u32 = 0b11 << 20;
    const FUNC_KIND: u32 = 0b01 << 20;

    const INDEX_MASK: u32 = (1 << 20) - 1; // 20 bits #19-#0 (if `indexed == 1`)

    /// A nullable untyped function reference aka `(ref null func)` aka
    /// `funcref` aka `anyfunc`.
    pub const FUNCREF: Self = RefType::FUNC.nullable();

    /// A nullable reference to an extern object aka `(ref null extern)` aka
    /// `externref`.
    pub const EXTERNREF: Self = RefType::EXTERN.nullable();

    /// A non-nullable untyped function reference aka `(ref func)`.
    pub const FUNC: Self = RefType::from_u32(Self::FUNC_TYPE);

    /// A non-nullable reference to an extern object aka `(ref extern)`.
    pub const EXTERN: Self = RefType::from_u32(Self::EXTERN_TYPE);

    /// A non-nullable reference to any object aka `(ref any)`.
    pub const ANY: Self = RefType::from_u32(Self::ANY_TYPE);

    /// A non-nullable reference to no object aka `(ref none)`.
    pub const NONE: Self = RefType::from_u32(Self::NONE_TYPE);

    /// A non-nullable reference to a noextern object aka `(ref noextern)`.
    pub const NOEXTERN: Self = RefType::from_u32(Self::NOEXTERN_TYPE);

    /// A non-nullable reference to a nofunc object aka `(ref nofunc)`.
    pub const NOFUNC: Self = RefType::from_u32(Self::NOFUNC_TYPE);

    /// A non-nullable reference to an eq object aka `(ref eq)`.
    pub const EQ: Self = RefType::from_u32(Self::EQ_TYPE);

    /// A non-nullable reference to a struct aka `(ref struct)`.
    pub const STRUCT: Self = RefType::from_u32(Self::STRUCT_TYPE);

    /// A non-nullable reference to an array aka `(ref array)`.
    pub const ARRAY: Self = RefType::from_u32(Self::ARRAY_TYPE);

    /// A non-nullable reference to an i31 object aka `(ref i31)`.
    pub const I31: Self = RefType::from_u32(Self::I31_TYPE);

    const fn can_represent_type_index(index: u32) -> bool {
        index & Self::INDEX_MASK == index
    }

    const fn u24_to_u32(bytes: [u8; 3]) -> u32 {
        let expanded_bytes = [bytes[0], bytes[1], bytes[2], 0];
        u32::from_le_bytes(expanded_bytes)
    }

    const fn u32_to_u24(x: u32) -> [u8; 3] {
        let bytes = x.to_le_bytes();
        debug_assert!(bytes[3] == 0);
        [bytes[0], bytes[1], bytes[2]]
    }

    #[inline]
    const fn as_u32(&self) -> u32 {
        Self::u24_to_u32(self.0)
    }

    #[inline]
    const fn from_u32(x: u32) -> Self {
        debug_assert!(x & (0b11111111 << 24) == 0);

        // if not indexed, type must be any/eq/i31/struct/array/func/extern/nofunc/noextern/none
        debug_assert!(
            x & Self::INDEXED_BIT != 0
                || matches!(
                    x & Self::TYPE_MASK,
                    Self::ANY_TYPE
                        | Self::EQ_TYPE
                        | Self::I31_TYPE
                        | Self::STRUCT_TYPE
                        | Self::ARRAY_TYPE
                        | Self::FUNC_TYPE
                        | Self::NOFUNC_TYPE
                        | Self::EXTERN_TYPE
                        | Self::NOEXTERN_TYPE
                        | Self::NONE_TYPE
                )
        );
        RefType(Self::u32_to_u24(x))
    }

    /// Create a reference to a typed function with the type at the given index.
    ///
    /// Returns `None` when the type index is beyond this crate's implementation
    /// limits and therefore is not representable.
    pub const fn indexed_func(nullable: bool, index: u32) -> Option<Self> {
        Self::indexed(nullable, Self::FUNC_KIND, index)
    }

    /// Create a reference to an array with the type at the given index.
    ///
    /// Returns `None` when the type index is beyond this crate's implementation
    /// limits and therefore is not representable.
    pub const fn indexed_array(nullable: bool, index: u32) -> Option<Self> {
        Self::indexed(nullable, Self::ARRAY_KIND, index)
    }

    /// Create a reference to a struct with the type at the given index.
    ///
    /// Returns `None` when the type index is beyond this crate's implementation
    /// limits and therefore is not representable.
    pub const fn indexed_struct(nullable: bool, index: u32) -> Option<Self> {
        Self::indexed(nullable, Self::STRUCT_KIND, index)
    }

    /// Create a reference to a user defined type at the given index.
    ///
    /// Returns `None` when the type index is beyond this crate's implementation
    /// limits and therefore is not representable, or when the heap type is not
    /// a typed array, struct or function.
    const fn indexed(nullable: bool, kind: u32, index: u32) -> Option<Self> {
        if Self::can_represent_type_index(index) {
            let nullable32 = Self::NULLABLE_BIT * nullable as u32;
            Some(RefType::from_u32(
                nullable32 | Self::INDEXED_BIT | kind | index,
            ))
        } else {
            None
        }
    }

    /// Create a new `RefType`.
    ///
    /// Returns `None` when the heap type's type index (if any) is beyond this
    /// crate's implementation limits and therfore is not representable.
    pub const fn new(nullable: bool, heap_type: HeapType) -> Option<Self> {
        let nullable32 = Self::NULLABLE_BIT * nullable as u32;
        match heap_type {
            HeapType::Indexed(index) => RefType::indexed(nullable, 0, index), // 0 bc we don't know the kind
            HeapType::Func => Some(Self::from_u32(nullable32 | Self::FUNC_TYPE)),
            HeapType::Extern => Some(Self::from_u32(nullable32 | Self::EXTERN_TYPE)),
            HeapType::Any => Some(Self::from_u32(nullable32 | Self::ANY_TYPE)),
            HeapType::None => Some(Self::from_u32(nullable32 | Self::NONE_TYPE)),
            HeapType::NoExtern => Some(Self::from_u32(nullable32 | Self::NOEXTERN_TYPE)),
            HeapType::NoFunc => Some(Self::from_u32(nullable32 | Self::NOFUNC_TYPE)),
            HeapType::Eq => Some(Self::from_u32(nullable32 | Self::EQ_TYPE)),
            HeapType::Struct => Some(Self::from_u32(nullable32 | Self::STRUCT_TYPE)),
            HeapType::Array => Some(Self::from_u32(nullable32 | Self::ARRAY_TYPE)),
            HeapType::I31 => Some(Self::from_u32(nullable32 | Self::I31_TYPE)),
        }
    }

    /// Is this a reference to a typed function?
    pub const fn is_typed_func_ref(&self) -> bool {
        self.is_indexed_type_ref() && self.as_u32() & Self::KIND_MASK == Self::FUNC_KIND
    }

    /// Is this a reference to an indexed type?
    pub const fn is_indexed_type_ref(&self) -> bool {
        self.as_u32() & Self::INDEXED_BIT != 0
    }

    /// If this is a reference to a typed function, get its type index.
    pub const fn type_index(&self) -> Option<u32> {
        if self.is_indexed_type_ref() {
            Some(self.as_u32() & Self::INDEX_MASK)
        } else {
            None
        }
    }

    /// Is this an untyped function reference aka `(ref null func)` aka `funcref` aka `anyfunc`?
    pub const fn is_func_ref(&self) -> bool {
        !self.is_indexed_type_ref() && self.as_u32() & Self::TYPE_MASK == Self::FUNC_TYPE
    }

    /// Is this a `(ref null extern)` aka `externref`?
    pub const fn is_extern_ref(&self) -> bool {
        !self.is_indexed_type_ref() && self.as_u32() & Self::TYPE_MASK == Self::EXTERN_TYPE
    }

    /// Is this ref type nullable?
    pub const fn is_nullable(&self) -> bool {
        self.as_u32() & Self::NULLABLE_BIT != 0
    }

    /// Get the non-nullable version of this ref type.
    pub const fn as_non_null(&self) -> Self {
        Self::from_u32(self.as_u32() & !Self::NULLABLE_BIT)
    }

    /// Get the non-nullable version of this ref type.
    pub const fn nullable(&self) -> Self {
        Self::from_u32(self.as_u32() | Self::NULLABLE_BIT)
    }

    /// Get the heap type that this is a reference to.
    pub fn heap_type(&self) -> HeapType {
        let s = self.as_u32();
        if self.is_indexed_type_ref() {
            HeapType::Indexed(self.type_index().unwrap())
        } else {
            match s & Self::TYPE_MASK {
                Self::FUNC_TYPE => HeapType::Func,
                Self::EXTERN_TYPE => HeapType::Extern,
                Self::ANY_TYPE => HeapType::Any,
                Self::NONE_TYPE => HeapType::None,
                Self::NOEXTERN_TYPE => HeapType::NoExtern,
                Self::NOFUNC_TYPE => HeapType::NoFunc,
                Self::EQ_TYPE => HeapType::Eq,
                Self::STRUCT_TYPE => HeapType::Struct,
                Self::ARRAY_TYPE => HeapType::Array,
                Self::I31_TYPE => HeapType::I31,
                _ => unreachable!(),
            }
        }
    }

    // Note that this is similar to `Display for RefType` except that it has
    // the indexes stubbed out.
    pub(crate) fn wat(&self) -> &'static str {
        match (self.is_nullable(), self.heap_type()) {
            (true, HeapType::Func) => "funcref",
            (true, HeapType::Extern) => "externref",
            (true, HeapType::Indexed(_)) => "(ref null $type)",
            (true, HeapType::Any) => "anyref",
            (true, HeapType::None) => "nullref",
            (true, HeapType::NoExtern) => "nullexternref",
            (true, HeapType::NoFunc) => "nullfuncref",
            (true, HeapType::Eq) => "eqref",
            (true, HeapType::Struct) => "structref",
            (true, HeapType::Array) => "arrayref",
            (true, HeapType::I31) => "i31ref",
            (false, HeapType::Func) => "(ref func)",
            (false, HeapType::Extern) => "(ref extern)",
            (false, HeapType::Indexed(_)) => "(ref $type)",
            (false, HeapType::Any) => "(ref any)",
            (false, HeapType::None) => "(ref none)",
            (false, HeapType::NoExtern) => "(ref noextern)",
            (false, HeapType::NoFunc) => "(ref nofunc)",
            (false, HeapType::Eq) => "(ref eq)",
            (false, HeapType::Struct) => "(ref struct)",
            (false, HeapType::Array) => "(ref array)",
            (false, HeapType::I31) => "(ref i31)",
        }
    }
}

impl Inherits for RefType {
    fn inherits<'a, F>(
        &self,
        other: &Self,
        self_base_idx: Option<u32>,
        other_base_idx: Option<u32>,
        type_at: &F,
    ) -> bool
    where
        F: Fn(u32) -> &'a SubType,
    {
        self == other
            || ((other.is_nullable() || !self.is_nullable())
                && self.heap_type().inherits(
                    &other.heap_type(),
                    self_base_idx,
                    other_base_idx,
                    type_at,
                ))
    }
}

impl<'a> FromReader<'a> for RefType {
    fn from_reader(reader: &mut BinaryReader<'a>) -> Result<Self> {
        match reader.read()? {
            0x70 => Ok(RefType::FUNC.nullable()),
            0x6F => Ok(RefType::EXTERN.nullable()),
            0x6E => Ok(RefType::ANY.nullable()),
            0x71 => Ok(RefType::NONE.nullable()),
            0x72 => Ok(RefType::NOEXTERN.nullable()),
            0x73 => Ok(RefType::NOFUNC.nullable()),
            0x6D => Ok(RefType::EQ.nullable()),
            0x6B => Ok(RefType::STRUCT.nullable()),
            0x6A => Ok(RefType::ARRAY.nullable()),
            0x6C => Ok(RefType::I31.nullable()),
            byte @ (0x63 | 0x64) => {
                let nullable = byte == 0x63;
                let pos = reader.original_position();
                RefType::new(nullable, reader.read()?)
                    .ok_or_else(|| crate::BinaryReaderError::new("type index too large", pos))
            }
            _ => bail!(reader.original_position(), "malformed reference type"),
        }
    }
}

impl fmt::Display for RefType {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        // Note that this is similar to `RefType::wat` except that it has the
        // indexes filled out.
        let s = match (self.is_nullable(), self.heap_type()) {
            (true, HeapType::Func) => "funcref",
            (true, HeapType::Extern) => "externref",
            (true, HeapType::Indexed(i)) => return write!(f, "(ref null {i})"),
            (true, HeapType::Any) => "anyref",
            (true, HeapType::None) => "nullref",
            (true, HeapType::NoExtern) => "nullexternref",
            (true, HeapType::NoFunc) => "nullfuncref",
            (true, HeapType::Eq) => "eqref",
            (true, HeapType::Struct) => "structref",
            (true, HeapType::Array) => "arrayref",
            (true, HeapType::I31) => "i31ref",
            (false, HeapType::Func) => "(ref func)",
            (false, HeapType::Extern) => "(ref extern)",
            (false, HeapType::Indexed(i)) => return write!(f, "(ref {i})"),
            (false, HeapType::Any) => "(ref any)",
            (false, HeapType::None) => "(ref none)",
            (false, HeapType::NoExtern) => "(ref noextern)",
            (false, HeapType::NoFunc) => "(ref nofunc)",
            (false, HeapType::Eq) => "(ref eq)",
            (false, HeapType::Struct) => "(ref struct)",
            (false, HeapType::Array) => "(ref array)",
            (false, HeapType::I31) => "(ref i31)",
        };
        f.write_str(s)
    }
}

/// A heap type from function references. When the proposal is disabled, Index
/// is an invalid type.
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum HeapType {
    /// User defined type at the given index.
    Indexed(u32),
    /// Untyped (any) function.
    Func,
    /// External heap type.
    Extern,
    /// The `any` heap type. The common supertype (a.k.a. top) of all internal types.
    Any,
    /// The `none` heap type. The common subtype (a.k.a. bottom) of all internal types.
    None,
    /// The `noextern` heap type. The common subtype (a.k.a. bottom) of all external types.
    NoExtern,
    /// The `nofunc` heap type. The common subtype (a.k.a. bottom) of all function types.
    NoFunc,
    /// The `eq` heap type. The common supertype of all referenceable types on which comparison
    /// (ref.eq) is allowed.
    Eq,
    /// The `struct` heap type. The common supertype of all struct types.
    Struct,
    /// The `array` heap type. The common supertype of all array types.
    Array,
    /// The i31 heap type.
    I31,
}

fn choose_base(base_idx: Option<u32>, idx: u32) -> Option<u32> {
    if base_idx == None || idx == base_idx.unwrap() || idx < base_idx.unwrap() {
        Some(idx)
    } else {
        base_idx
    }
}

impl Inherits for HeapType {
    fn inherits<'a, F>(
        &self,
        other: &Self,
        self_base_idx: Option<u32>,
        other_base_idx: Option<u32>,
        type_at: &F,
    ) -> bool
    where
        F: Fn(u32) -> &'a SubType,
    {
        match (self, other) {
            (HeapType::Indexed(a), HeapType::Indexed(b)) => {
                a == b || {
                    let a_base = choose_base(self_base_idx, *a);
                    let b_base = choose_base(other_base_idx, *b);
                    (a_base == self_base_idx && b_base == other_base_idx)
                        || type_at(*a).inherits(type_at(*b), a_base, b_base, type_at)
                }
            }
            (HeapType::Indexed(a), HeapType::Func) => match type_at(*a).structural_type {
                StructuralType::Func(_) => true,
                _ => false,
            },
            (HeapType::Indexed(a), HeapType::Array) => match type_at(*a).structural_type {
                StructuralType::Array(_) => true,
                _ => false,
            },
            (HeapType::Indexed(a), HeapType::Struct) => match type_at(*a).structural_type {
                StructuralType::Struct(_) => true,
                _ => false,
            },
            (HeapType::Indexed(a), HeapType::Eq | HeapType::Any) => {
                match type_at(*a).structural_type {
                    StructuralType::Array(_) | StructuralType::Struct(_) => true,
                    _ => false,
                }
            }
            (HeapType::Eq, HeapType::Any) => true,
            (HeapType::I31 | HeapType::Array | HeapType::Struct, HeapType::Eq | HeapType::Any) => {
                true
            }
            (HeapType::None, HeapType::Indexed(a)) => match type_at(*a).structural_type {
                StructuralType::Array(_) | StructuralType::Struct(_) => true,
                _ => false,
            },
            (
                HeapType::None,
                HeapType::I31 | HeapType::Eq | HeapType::Any | HeapType::Array | HeapType::Struct,
            ) => true,
            (HeapType::NoExtern, HeapType::Extern) => true,
            (HeapType::NoFunc, HeapType::Func) => true,
            (HeapType::NoFunc, HeapType::Indexed(a)) => match type_at(*a).structural_type {
                StructuralType::Func(_) => true,
                _ => false,
            },
            (
                a @ (HeapType::Func
                | HeapType::Extern
                | HeapType::Any
                | HeapType::Indexed(_)
                | HeapType::None
                | HeapType::NoExtern
                | HeapType::NoFunc
                | HeapType::Eq
                | HeapType::Struct
                | HeapType::Array
                | HeapType::I31),
                b,
            ) => a == b,
        }
    }
}

impl<'a> FromReader<'a> for HeapType {
    fn from_reader(reader: &mut BinaryReader<'a>) -> Result<Self> {
        match reader.peek()? {
            0x70 => {
                reader.position += 1;
                Ok(HeapType::Func)
            }
            0x6F => {
                reader.position += 1;
                Ok(HeapType::Extern)
            }
            0x6E => {
                reader.position += 1;
                Ok(HeapType::Any)
            }
            0x71 => {
                reader.position += 1;
                Ok(HeapType::None)
            }
            0x72 => {
                reader.position += 1;
                Ok(HeapType::NoExtern)
            }
            0x73 => {
                reader.position += 1;
                Ok(HeapType::NoFunc)
            }
            0x6D => {
                reader.position += 1;
                Ok(HeapType::Eq)
            }
            0x6B => {
                reader.position += 1;
                Ok(HeapType::Struct)
            }
            0x6A => {
                reader.position += 1;
                Ok(HeapType::Array)
            }
            0x6C => {
                reader.position += 1;
                Ok(HeapType::I31)
            }
            _ => {
                let idx = match u32::try_from(reader.read_var_s33()?) {
                    Ok(idx) => idx,
                    Err(_) => {
                        bail!(reader.original_position(), "invalid indexed ref heap type");
                    }
                };
                Ok(HeapType::Indexed(idx))
            }
        }
    }
}

/// Represents a structural type in a WebAssembly module.
#[derive(Debug, Clone)]
pub enum StructuralType {
    /// The type is for a function.
    Func(FuncType),
    /// The type is for an array.
    Array(ArrayType),
    /// The type is for a struct.
    Struct(StructType),
}

/// Represents a subtype of possible other types in a WebAssembly module.
#[derive(Debug, Clone)]
pub struct SubType {
    /// Is the subtype final.
    pub is_final: bool,
    /// The list of supertype indexes. As of GC MVP, there can be at most one supertype.
    pub supertype_idx: Option<u32>,
    /// The structural type of the subtype.
    pub structural_type: StructuralType,
}

/// Represents a recursive type group in a WebAssembly module.
#[derive(Debug, Clone)]
pub enum RecGroup {
    /// The list of subtypes in the recursive type group.
    Many(Vec<SubType>),
    /// A single subtype in the recursive type group.
    Single(SubType),
}

impl RecGroup {
    /// Returns the list of subtypes in the recursive type group.
    pub fn types(&self) -> &[SubType] {
        match self {
            RecGroup::Many(types) => types,
            RecGroup::Single(ty) => slice::from_ref(ty),
        }
    }
}

impl Inherits for SubType {
    fn inherits<'a, F>(
        &self,
        other: &Self,
        self_base_idx: Option<u32>,
        other_base_idx: Option<u32>,
        type_at: &F,
    ) -> bool
    where
        F: Fn(u32) -> &'a SubType,
    {
        !other.is_final
            && self.structural_type.inherits(
                &other.structural_type,
                self_base_idx,
                other_base_idx,
                type_at,
            )
    }
}

/// Represents a type of a function in a WebAssembly module.
#[derive(Clone, Eq, PartialEq, Hash)]
pub struct FuncType {
    /// The combined parameters and result types.
    params_results: Box<[ValType]>,
    /// The number of parameter types.
    len_params: usize,
}

/// Represents a type of an array in a WebAssembly module.
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub struct ArrayType(pub FieldType);

/// Represents a field type of an array or a struct.
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub struct FieldType {
    /// Array element type.
    pub element_type: StorageType,
    /// Are elements mutable.
    pub mutable: bool,
}

/// Represents a type of a struct in a WebAssembly module.
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub struct StructType {
    /// Struct fields.
    pub fields: Box<[FieldType]>,
}

impl Inherits for StructuralType {
    fn inherits<'a, F>(
        &self,
        other: &Self,
        self_base_idx: Option<u32>,
        other_base_idx: Option<u32>,
        type_at: &F,
    ) -> bool
    where
        F: Fn(u32) -> &'a SubType,
    {
        match (self, other) {
            (StructuralType::Func(a), StructuralType::Func(b)) => {
                a.inherits(b, self_base_idx, other_base_idx, type_at)
            }
            (StructuralType::Array(a), StructuralType::Array(b)) => {
                a.inherits(b, self_base_idx, other_base_idx, type_at)
            }
            (StructuralType::Struct(a), StructuralType::Struct(b)) => {
                a.inherits(b, self_base_idx, other_base_idx, type_at)
            }
            (StructuralType::Func(_), _) => false,
            (StructuralType::Array(_), _) => false,
            (StructuralType::Struct(_), _) => false,
        }
    }
}

impl Debug for FuncType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("FuncType")
            .field("params", &self.params())
            .field("returns", &self.results())
            .finish()
    }
}

impl FuncType {
    /// Creates a new [`FuncType`] from the given `params` and `results`.
    pub fn new<P, R>(params: P, results: R) -> Self
    where
        P: IntoIterator<Item = ValType>,
        R: IntoIterator<Item = ValType>,
    {
        let mut buffer = params.into_iter().collect::<Vec<_>>();
        let len_params = buffer.len();
        buffer.extend(results);
        Self {
            params_results: buffer.into(),
            len_params,
        }
    }

    /// Creates a new [`FuncType`] fom its raw parts.
    ///
    /// # Panics
    ///
    /// If `len_params` is greater than the length of `params_results` combined.
    pub(crate) fn from_raw_parts(params_results: Box<[ValType]>, len_params: usize) -> Self {
        assert!(len_params <= params_results.len());
        Self {
            params_results,
            len_params,
        }
    }

    /// Returns a shared slice to the parameter types of the [`FuncType`].
    #[inline]
    pub fn params(&self) -> &[ValType] {
        &self.params_results[..self.len_params]
    }

    /// Returns a shared slice to the result types of the [`FuncType`].
    #[inline]
    pub fn results(&self) -> &[ValType] {
        &self.params_results[self.len_params..]
    }

    pub(crate) fn desc(&self) -> String {
        let mut s = String::new();
        s.push_str("[");
        for (i, param) in self.params().iter().enumerate() {
            if i > 0 {
                s.push_str(" ");
            }
            write!(s, "{param}").unwrap();
        }
        s.push_str("] -> [");
        for (i, result) in self.results().iter().enumerate() {
            if i > 0 {
                s.push_str(" ");
            }
            write!(s, "{result}").unwrap();
        }
        s.push_str("]");
        s
    }
}

impl Inherits for FuncType {
    fn inherits<'a, F>(
        &self,
        other: &Self,
        self_base_idx: Option<u32>,
        other_base_idx: Option<u32>,
        type_at: &F,
    ) -> bool
    where
        F: Fn(u32) -> &'a SubType,
    {
        self.params().len() == other.params().len()
            && self.results().len() == other.results().len()
            // Note: per GC spec, function subtypes are contravariant in their parameter types.
            // Also see https://en.wikipedia.org/wiki/Covariance_and_contravariance_(computer_science)
            && self
                .params()
                .iter()
                .zip(other.params())
                .fold(true, |r, (a, b)| r && b.inherits(a, self_base_idx, other_base_idx, type_at))
            && self
                .results()
                .iter()
                .zip(other.results())
                .fold(true, |r, (a, b)| r && a.inherits(b, self_base_idx, other_base_idx, type_at))
    }
}

impl Inherits for ArrayType {
    fn inherits<'a, F>(
        &self,
        other: &Self,
        self_base_idx: Option<u32>,
        other_base_idx: Option<u32>,
        type_at: &F,
    ) -> bool
    where
        F: Fn(u32) -> &'a SubType,
    {
        self.0
            .inherits(&other.0, self_base_idx, other_base_idx, type_at)
    }
}

impl Inherits for FieldType {
    fn inherits<'a, F>(
        &self,
        other: &Self,
        self_base_idx: Option<u32>,
        other_base_idx: Option<u32>,
        type_at: &F,
    ) -> bool
    where
        F: Fn(u32) -> &'a SubType,
    {
        (other.mutable || !self.mutable)
            && self.element_type.inherits(
                &other.element_type,
                self_base_idx,
                other_base_idx,
                type_at,
            )
    }
}

impl Inherits for StorageType {
    fn inherits<'a, F>(
        &self,
        other: &Self,
        self_base_idx: Option<u32>,
        other_base_idx: Option<u32>,
        type_at: &F,
    ) -> bool
    where
        F: Fn(u32) -> &'a SubType,
    {
        match (self, other) {
            (Self::Val(a), Self::Val(b)) => a.inherits(b, self_base_idx, other_base_idx, type_at),
            (a @ (Self::I8 | Self::I16 | Self::Val(_)), b) => a == b,
        }
    }
}

impl Inherits for StructType {
    fn inherits<'a, F>(
        &self,
        other: &Self,
        self_base_idx: Option<u32>,
        other_base_idx: Option<u32>,
        type_at: &F,
    ) -> bool
    where
        F: Fn(u32) -> &'a SubType,
    {
        // Note: Structure types support width and depth subtyping.
        self.fields.len() >= other.fields.len()
            && self
                .fields
                .iter()
                .zip(other.fields.iter())
                .fold(true, |r, (a, b)| {
                    r && a.inherits(b, self_base_idx, other_base_idx, type_at)
                })
    }
}

/// Represents a table's type.
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub struct TableType {
    /// The table's element type.
    pub element_type: RefType,
    /// Initial size of this table, in elements.
    pub initial: u32,
    /// Optional maximum size of the table, in elements.
    pub maximum: Option<u32>,
}

/// Represents a memory's type.
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub struct MemoryType {
    /// Whether or not this is a 64-bit memory, using i64 as an index. If this
    /// is false it's a 32-bit memory using i32 as an index.
    ///
    /// This is part of the memory64 proposal in WebAssembly.
    pub memory64: bool,

    /// Whether or not this is a "shared" memory, indicating that it should be
    /// send-able across threads and the `maximum` field is always present for
    /// valid types.
    ///
    /// This is part of the threads proposal in WebAssembly.
    pub shared: bool,

    /// Initial size of this memory, in wasm pages.
    ///
    /// For 32-bit memories (when `memory64` is `false`) this is guaranteed to
    /// be at most `u32::MAX` for valid types.
    pub initial: u64,

    /// Optional maximum size of this memory, in wasm pages.
    ///
    /// For 32-bit memories (when `memory64` is `false`) this is guaranteed to
    /// be at most `u32::MAX` for valid types. This field is always present for
    /// valid wasm memories when `shared` is `true`.
    pub maximum: Option<u64>,
}

impl MemoryType {
    /// Gets the index type for the memory.
    pub fn index_type(&self) -> ValType {
        if self.memory64 {
            ValType::I64
        } else {
            ValType::I32
        }
    }
}

/// Represents a global's type.
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub struct GlobalType {
    /// The global's type.
    pub content_type: ValType,
    /// Whether or not the global is mutable.
    pub mutable: bool,
}

/// Represents a tag kind.
#[derive(Clone, Copy, Debug)]
pub enum TagKind {
    /// The tag is an exception type.
    Exception,
}

/// A tag's type.
#[derive(Clone, Copy, Debug)]
pub struct TagType {
    /// The kind of tag
    pub kind: TagKind,
    /// The function type this tag uses.
    pub func_type_idx: u32,
}

/// A reader for the type section of a WebAssembly module.
pub type TypeSectionReader<'a> = SectionLimited<'a, RecGroup>;

impl<'a> TypeSectionReader<'a> {
    /// Returns an iterator over this type section which will only yield
    /// function types and any usage of GC types from the GC proposal will
    /// be translated into an error.
    pub fn into_iter_err_on_gc_types(self) -> impl Iterator<Item = Result<FuncType>> + 'a {
        self.into_iter_with_offsets().map(|item| {
            let (offset, group) = item?;
            let ty = match group {
                RecGroup::Single(ty) => ty,
                RecGroup::Many(_) => bail!(offset, "gc proposal not supported"),
            };
            if !ty.is_final || ty.supertype_idx.is_some() {
                bail!(offset, "gc proposal not supported");
            }
            match ty.structural_type {
                StructuralType::Func(f) => Ok(f),
                StructuralType::Array(_) | StructuralType::Struct(_) => {
                    bail!(offset, "gc proposal not supported");
                }
            }
        })
    }
}

impl<'a> FromReader<'a> for StructuralType {
    fn from_reader(reader: &mut BinaryReader<'a>) -> Result<Self> {
        read_structural_type(reader.read_u8()?, reader)
    }
}

fn read_structural_type(
    opcode: u8,
    reader: &mut BinaryReader,
) -> Result<StructuralType, BinaryReaderError> {
    Ok(match opcode {
        0x60 => StructuralType::Func(reader.read()?),
        0x5e => StructuralType::Array(reader.read()?),
        0x5f => StructuralType::Struct(reader.read()?),
        x => return reader.invalid_leading_byte(x, "type"),
    })
}

impl<'a> FromReader<'a> for RecGroup {
    fn from_reader(reader: &mut BinaryReader<'a>) -> Result<Self> {
        Ok(match reader.peek()? {
            0x4e => {
                reader.read_u8()?;
                let types = reader.read_iter(MAX_WASM_TYPES, "rec group types")?;
                RecGroup::Many(types.collect::<Result<_>>()?)
            }
            _ => RecGroup::Single(reader.read()?),
        })
    }
}

impl<'a> FromReader<'a> for SubType {
    fn from_reader(reader: &mut BinaryReader<'a>) -> Result<Self> {
        let pos = reader.original_position();
        Ok(match reader.read_u8()? {
            opcode @ (0x4f | 0x50) => {
                let idx_iter = reader.read_iter(MAX_WASM_SUPERTYPES, "supertype idxs")?;
                let idxs = idx_iter.collect::<Result<Vec<u32>>>()?;
                if idxs.len() > 1 {
                    return Err(BinaryReaderError::new(
                        "multiple supertypes not supported",
                        pos,
                    ));
                }
                SubType {
                    is_final: opcode == 0x4f,
                    supertype_idx: idxs.first().copied(),
                    structural_type: read_structural_type(reader.read_u8()?, reader)?,
                }
            }
            opcode => SubType {
                is_final: true,
                supertype_idx: None,
                structural_type: read_structural_type(opcode, reader)?,
            },
        })
    }
}

impl<'a> FromReader<'a> for FuncType {
    fn from_reader(reader: &mut BinaryReader<'a>) -> Result<Self> {
        let mut params_results = reader
            .read_iter(MAX_WASM_FUNCTION_PARAMS, "function params")?
            .collect::<Result<Vec<_>>>()?;
        let len_params = params_results.len();
        let results = reader.read_iter(MAX_WASM_FUNCTION_RETURNS, "function returns")?;
        params_results.reserve(results.size_hint().0);
        for result in results {
            params_results.push(result?);
        }
        Ok(FuncType::from_raw_parts(params_results.into(), len_params))
    }
}

impl<'a> FromReader<'a> for FieldType {
    fn from_reader(reader: &mut BinaryReader<'a>) -> Result<Self> {
        let element_type = reader.read()?;
        let mutable = reader.read_u8()?;
        Ok(FieldType {
            element_type,
            mutable: match mutable {
                0 => false,
                1 => true,
                _ => bail!(
                    reader.original_position(),
                    "invalid mutability byte for array type"
                ),
            },
        })
    }
}

impl<'a> FromReader<'a> for ArrayType {
    fn from_reader(reader: &mut BinaryReader<'a>) -> Result<Self> {
        Ok(ArrayType(FieldType::from_reader(reader)?))
    }
}

impl<'a> FromReader<'a> for StructType {
    fn from_reader(reader: &mut BinaryReader<'a>) -> Result<Self> {
        let fields = reader.read_iter(MAX_WASM_STRUCT_FIELDS, "struct fields")?;
        Ok(StructType {
            fields: fields.collect::<Result<_>>()?,
        })
    }
}
