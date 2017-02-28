use error::Error;
use num_traits::NumCast;
use serde::de::{self, Visitor};
use serde::{Serialize, Serializer, Deserialize, Deserializer};
use std::fmt::{self, Debug, Display};
use std::i64;

/// Represents a JSON number, whether integer or floating point.
#[derive(Clone, PartialEq)]
pub struct Number {
    n: N,
}

// "N" is a prefix of "NegInt"... this is a false positive.
// https://github.com/Manishearth/rust-clippy/issues/1241
#[cfg_attr(feature = "cargo-clippy", allow(enum_variant_names))]
#[derive(Copy, Clone, Debug, PartialEq)]
enum N {
    PosInt(u64),
    /// Always less than zero.
    NegInt(i64),
    /// Always finite.
    Float(f64),
}

impl Number {
    /// Returns `true` if the number can be represented as `i64`.
    #[inline]
    pub fn is_i64(&self) -> bool {
        match self.n {
            N::PosInt(v) => v <= i64::MAX as u64,
            N::NegInt(_) => true,
            N::Float(_) => false,
        }
    }

    /// Returns `true` if the number can be represented as `u64`.
    #[inline]
    pub fn is_u64(&self) -> bool {
        match self.n {
            N::PosInt(_) => true,
            N::NegInt(_) | N::Float(_) => false,
        }
    }

    /// Returns `true` if the number can be represented as `f64`.
    #[inline]
    pub fn is_f64(&self) -> bool {
        match self.n {
            N::Float(_) => true,
            N::PosInt(_) | N::NegInt(_) => false,
        }
    }

    /// Returns the number represented as `i64` if possible, or else `None`.
    #[inline]
    pub fn as_i64(&self) -> Option<i64> {
        match self.n {
            N::PosInt(n) => NumCast::from(n),
            N::NegInt(n) => Some(n),
            N::Float(_) => None,
        }
    }

    /// Returns the number represented as `u64` if possible, or else `None`.
    #[inline]
    pub fn as_u64(&self) -> Option<u64> {
        match self.n {
            N::PosInt(n) => Some(n),
            N::NegInt(n) => NumCast::from(n),
            N::Float(_) => None,
        }
    }

    /// Returns the number represented as `f64` if possible, or else `None`.
    #[inline]
    pub fn as_f64(&self) -> Option<f64> {
        match self.n {
            N::PosInt(n) => NumCast::from(n),
            N::NegInt(n) => NumCast::from(n),
            N::Float(n) => Some(n),
        }
    }

    /// Converts a finite `f64` to a `Number`. Infinite or NaN values are not JSON
    /// numbers.
    #[inline]
    pub fn from_f64(f: f64) -> Option<Number> {
        if f.is_finite() {
            Some(Number { n: N::Float(f) })
        } else {
            None
        }
    }
}

impl fmt::Display for Number {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        match self.n {
            N::PosInt(i) => Display::fmt(&i, formatter),
            N::NegInt(i) => Display::fmt(&i, formatter),
            N::Float(f) => Display::fmt(&f, formatter),
        }
    }
}

impl Debug for Number {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        Debug::fmt(&self.n, formatter)
    }
}

impl Serialize for Number {
    #[inline]
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
        where S: Serializer
    {
        match self.n {
            N::PosInt(i) => serializer.serialize_u64(i),
            N::NegInt(i) => serializer.serialize_i64(i),
            N::Float(f) => serializer.serialize_f64(f),
        }
    }
}

impl Deserialize for Number {
    #[inline]
    fn deserialize<D>(deserializer: D) -> Result<Number, D::Error>
        where D: Deserializer
    {
        struct NumberVisitor;

        impl Visitor for NumberVisitor {
            type Value = Number;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("a number")
            }

            #[inline]
            fn visit_i64<E>(self, value: i64) -> Result<Number, E> {
                Ok(value.into())
            }

            #[inline]
            fn visit_u64<E>(self, value: u64) -> Result<Number, E> {
                Ok(value.into())
            }

            #[inline]
            fn visit_f64<E>(self, value: f64) -> Result<Number, E>
                where E: de::Error
            {
                Number::from_f64(value).ok_or_else(|| de::Error::custom("not a JSON number"))
            }
        }

        deserializer.deserialize(NumberVisitor)
    }
}

impl Deserializer for Number {
    type Error = Error;

    #[inline]
    fn deserialize<V>(self, visitor: V) -> Result<V::Value, Error>
        where V: Visitor
    {
        match self.n {
            N::PosInt(i) => visitor.visit_u64(i),
            N::NegInt(i) => visitor.visit_i64(i),
            N::Float(f) => visitor.visit_f64(f),
        }
    }

    forward_to_deserialize! {
        bool u8 u16 u32 u64 i8 i16 i32 i64 f32 f64 char str string unit option
        seq seq_fixed_size bytes byte_buf map unit_struct newtype_struct
        tuple_struct struct struct_field tuple enum ignored_any
    }
}

impl<'a> Deserializer for &'a Number {
    type Error = Error;

    #[inline]
    fn deserialize<V>(self, visitor: V) -> Result<V::Value, Error>
        where V: Visitor
    {
        match self.n {
            N::PosInt(i) => visitor.visit_u64(i),
            N::NegInt(i) => visitor.visit_i64(i),
            N::Float(f) => visitor.visit_f64(f),
        }
    }

    forward_to_deserialize! {
        bool u8 u16 u32 u64 i8 i16 i32 i64 f32 f64 char str string unit option
        seq seq_fixed_size bytes byte_buf map unit_struct newtype_struct
        tuple_struct struct struct_field tuple enum ignored_any
    }
}

macro_rules! from_signed {
    ($($signed_ty:ident)*) => {
        $(
            impl From<$signed_ty> for Number {
                #[inline]
                fn from(i: $signed_ty) -> Self {
                    if i < 0 {
                        Number { n: N::NegInt(i as i64) }
                    } else {
                        Number { n: N::PosInt(i as u64) }
                    }
                }
            }
        )*
    };
}

macro_rules! from_unsigned {
    ($($unsigned_ty:ident)*) => {
        $(
            impl From<$unsigned_ty> for Number {
                #[inline]
                fn from(u: $unsigned_ty) -> Self {
                    Number { n: N::PosInt(u as u64) }
                }
            }
        )*
    };
}

from_signed!(i8 i16 i32 i64 isize);
from_unsigned!(u8 u16 u32 u64 usize);
