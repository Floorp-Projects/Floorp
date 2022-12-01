//! CBOR values, keys and serialization routines.

mod de;
mod ser;

use std::cmp::{Ord, Ordering, PartialOrd};
use std::collections::BTreeMap;

#[doc(inline)]
pub use self::de::from_value;
#[doc(inline)]
pub use self::ser::to_value;

/// The `Value` enum, a loosely typed way of representing any valid CBOR value.
///
/// Maps are sorted according to the canonical ordering
/// described in [RFC 7049 bis].
/// Therefore values are unambiguously serialized
/// to a canonical form of CBOR from the same RFC.
///
/// [RFC 7049 bis]: https://tools.ietf.org/html/draft-ietf-cbor-7049bis-04#section-2
#[derive(Clone, Debug)]
pub enum Value {
    /// Represents the absence of a value or the value undefined.
    Null,
    /// Represents a boolean value.
    Bool(bool),
    /// Integer CBOR numbers.
    ///
    /// The biggest value that can be represented is 2^64 - 1.
    /// While the smallest value is -2^64.
    /// Values outside this range can't be serialized
    /// and will cause an error.
    Integer(i128),
    /// Represents a floating point value.
    Float(f64),
    /// Represents a byte string.
    Bytes(Vec<u8>),
    /// Represents an UTF-8 encoded string.
    Text(String),
    /// Represents an array of values.
    Array(Vec<Value>),
    /// Represents a map.
    ///
    /// Maps are also called tables, dictionaries, hashes, or objects (in JSON).
    /// While any value can be used as a CBOR key
    /// it is better to use only one type of key in a map
    /// to avoid ambiguity.
    /// If floating point values are used as keys they are compared bit-by-bit for equality.
    /// If arrays or maps are used as keys the comparisons
    /// to establish canonical order may be slow and therefore insertion
    /// and retrieval of values will be slow too.
    Map(BTreeMap<Value, Value>),
    /// Represents a tagged value
    Tag(u64, Box<Value>),
    // The hidden variant allows the enum to be extended
    // with variants for tags and simple values.
    #[doc(hidden)]
    __Hidden,
}

impl PartialEq for Value {
    fn eq(&self, other: &Value) -> bool {
        self.cmp(other) == Ordering::Equal
    }
}

impl Eq for Value {}

impl PartialOrd for Value {
    fn partial_cmp(&self, other: &Value) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for Value {
    fn cmp(&self, other: &Value) -> Ordering {
        // Determine the canonical order of two values:
        // 1. Smaller major type sorts first.
        // 2. Shorter sequence sorts first.
        // 3. Compare integers by magnitude.
        // 4. Compare byte and text sequences lexically.
        // 5. Compare the serializations of both types. (expensive)
        use self::Value::*;
        if self.major_type() != other.major_type() {
            return self.major_type().cmp(&other.major_type());
        }
        match (self, other) {
            (Integer(a), Integer(b)) => a.abs().cmp(&b.abs()),
            (Bytes(a), Bytes(b)) if a.len() != b.len() => a.len().cmp(&b.len()),
            (Text(a), Text(b)) if a.len() != b.len() => a.len().cmp(&b.len()),
            (Array(a), Array(b)) if a.len() != b.len() => a.len().cmp(&b.len()),
            (Map(a), Map(b)) if a.len() != b.len() => a.len().cmp(&b.len()),
            (Bytes(a), Bytes(b)) => a.cmp(b),
            (Text(a), Text(b)) => a.cmp(b),
            (a, b) => {
                let a = crate::to_vec(a).expect("self is serializable");
                let b = crate::to_vec(b).expect("other is serializable");
                a.cmp(&b)
            }
        }
    }
}

macro_rules! impl_from {
    ($variant:path, $for_type:ty) => {
        impl From<$for_type> for Value {
            fn from(v: $for_type) -> Value {
                $variant(v.into())
            }
        }
    };
}

impl_from!(Value::Bool, bool);
impl_from!(Value::Integer, i8);
impl_from!(Value::Integer, i16);
impl_from!(Value::Integer, i32);
impl_from!(Value::Integer, i64);
// i128 omitted because not all numbers fit in CBOR serialization
impl_from!(Value::Integer, u8);
impl_from!(Value::Integer, u16);
impl_from!(Value::Integer, u32);
impl_from!(Value::Integer, u64);
// u128 omitted because not all numbers fit in CBOR serialization
impl_from!(Value::Float, f32);
impl_from!(Value::Float, f64);
impl_from!(Value::Bytes, Vec<u8>);
impl_from!(Value::Text, String);
// TODO: figure out if these impls should be more generic or removed.
impl_from!(Value::Array, Vec<Value>);
impl_from!(Value::Map, BTreeMap<Value, Value>);

impl Value {
    fn major_type(&self) -> u8 {
        use self::Value::*;
        match self {
            Null => 7,
            Bool(_) => 7,
            Integer(v) => {
                if *v >= 0 {
                    0
                } else {
                    1
                }
            }
            Tag(_, _) => 6,
            Float(_) => 7,
            Bytes(_) => 2,
            Text(_) => 3,
            Array(_) => 4,
            Map(_) => 5,
            __Hidden => unreachable!(),
        }
    }
}
