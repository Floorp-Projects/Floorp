//! Support for cbor tags
use core::fmt;
use core::marker::PhantomData;
use serde::de::{
    Deserialize, Deserializer, EnumAccess, IntoDeserializer, MapAccess, SeqAccess, Visitor,
};
use serde::forward_to_deserialize_any;
use serde::ser::{Serialize, Serializer};

/// signals that a newtype is from a CBOR tag
pub(crate) const CBOR_NEWTYPE_NAME: &str = "\0cbor_tag";

/// A value that is optionally tagged with a cbor tag
///
/// this only serves as an intermediate helper for tag serialization or deserialization
pub struct Tagged<T> {
    /// cbor tag
    pub tag: Option<u64>,
    /// value
    pub value: T,
}

impl<T> Tagged<T> {
    /// Create a new tagged value
    pub fn new(tag: Option<u64>, value: T) -> Self {
        Self { tag, value }
    }
}

impl<T: Serialize> Serialize for Tagged<T> {
    fn serialize<S: Serializer>(&self, s: S) -> Result<S::Ok, S::Error> {
        set_tag(self.tag);
        let r = s.serialize_newtype_struct(CBOR_NEWTYPE_NAME, &self.value);
        set_tag(None);
        r
    }
}

fn untagged<T>(value: T) -> Tagged<T> {
    Tagged::new(None, value)
}

macro_rules! delegate {
    ($name: ident, $type: ty) => {
        fn $name<E: serde::de::Error>(self, v: $type) -> Result<Self::Value, E>
        {
            T::deserialize(v.into_deserializer()).map(untagged)
        }
    };
}

struct EnumDeserializer<A>(A);

impl<'de, A> Deserializer<'de> for EnumDeserializer<A>
where
    A: EnumAccess<'de>,
{
    type Error = A::Error;

    fn deserialize_any<V: Visitor<'de>>(self, visitor: V) -> Result<V::Value, Self::Error> {
        visitor.visit_enum(self.0)
    }

    forward_to_deserialize_any! {
        bool i8 i16 i32 i64 i128 u8 u16 u32 u64 u128 f32 f64 char str string
        bytes byte_buf option unit unit_struct newtype_struct seq tuple
        tuple_struct map struct enum identifier ignored_any
    }
}

struct NoneDeserializer<E>(PhantomData<E>);

impl<'de, E> Deserializer<'de> for NoneDeserializer<E>
where
    E: serde::de::Error,
{
    type Error = E;

    fn deserialize_any<V: Visitor<'de>>(self, visitor: V) -> Result<V::Value, Self::Error> {
        visitor.visit_none()
    }

    forward_to_deserialize_any! {
        bool i8 i16 i32 i64 i128 u8 u16 u32 u64 u128 f32 f64 char str string
        bytes byte_buf option unit unit_struct newtype_struct seq tuple
        tuple_struct map struct enum identifier ignored_any
    }
}

struct BytesDeserializer<'a, E>(&'a [u8], PhantomData<E>);

impl<'de, 'a, E> Deserializer<'de> for BytesDeserializer<'a, E>
where
    E: serde::de::Error,
{
    type Error = E;

    fn deserialize_any<V: Visitor<'de>>(self, visitor: V) -> Result<V::Value, Self::Error> {
        visitor.visit_bytes(self.0)
    }

    forward_to_deserialize_any! {
        bool i8 i16 i32 i64 i128 u8 u16 u32 u64 u128 f32 f64 char str string
        bytes byte_buf option unit unit_struct newtype_struct seq tuple
        tuple_struct map struct enum identifier ignored_any
    }
}

/// A visitor that intercepts *just* visit_newtype_struct and passes through everything else.
struct MaybeTaggedVisitor<T>(PhantomData<T>);

impl<'de, T: Deserialize<'de>> Visitor<'de> for MaybeTaggedVisitor<T> {
    type Value = Tagged<T>;

    fn expecting(&self, fmt: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt.write_str("a cbor tag newtype")
    }

    delegate!(visit_bool, bool);

    delegate!(visit_i8, i8);
    delegate!(visit_i16, i16);
    delegate!(visit_i32, i32);
    delegate!(visit_i64, i64);

    delegate!(visit_u8, u8);
    delegate!(visit_u16, u16);
    delegate!(visit_u32, u32);
    delegate!(visit_u64, u64);

    delegate!(visit_f32, f32);
    delegate!(visit_f64, f64);

    delegate!(visit_char, char);
    delegate!(visit_str, &str);
    delegate!(visit_borrowed_str, &'de str);

    #[cfg(feature = "std")]
    delegate!(visit_byte_buf, Vec<u8>);

    #[cfg(feature = "std")]
    delegate!(visit_string, String);

    fn visit_bytes<E: serde::de::Error>(self, value: &[u8]) -> Result<Self::Value, E> {
        T::deserialize(BytesDeserializer(value, PhantomData)).map(untagged)
    }

    fn visit_borrowed_bytes<E: serde::de::Error>(self, value: &'de [u8]) -> Result<Self::Value, E> {
        T::deserialize(serde::de::value::BorrowedBytesDeserializer::new(value)).map(untagged)
    }

    fn visit_unit<E: serde::de::Error>(self) -> Result<Self::Value, E> {
        T::deserialize(().into_deserializer()).map(untagged)
    }

    fn visit_none<E: serde::de::Error>(self) -> Result<Self::Value, E> {
        T::deserialize(NoneDeserializer(PhantomData)).map(untagged)
    }

    fn visit_some<D: Deserializer<'de>>(self, deserializer: D) -> Result<Self::Value, D::Error> {
        T::deserialize(deserializer).map(untagged)
    }

    fn visit_seq<A: SeqAccess<'de>>(self, seq: A) -> Result<Self::Value, A::Error> {
        T::deserialize(serde::de::value::SeqAccessDeserializer::new(seq)).map(untagged)
    }

    fn visit_map<V: MapAccess<'de>>(self, map: V) -> Result<Self::Value, V::Error> {
        T::deserialize(serde::de::value::MapAccessDeserializer::new(map)).map(untagged)
    }

    fn visit_enum<A: EnumAccess<'de>>(self, data: A) -> Result<Self::Value, A::Error> {
        T::deserialize(EnumDeserializer(data)).map(untagged)
    }

    fn visit_newtype_struct<D: serde::Deserializer<'de>>(
        self,
        deserializer: D,
    ) -> Result<Self::Value, D::Error> {
        let t = get_tag();
        T::deserialize(deserializer).map(|v| Tagged::new(t, v))
    }
}

impl<'de, T: serde::de::Deserialize<'de>> serde::de::Deserialize<'de> for Tagged<T> {
    fn deserialize<D: serde::de::Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        deserializer.deserialize_any(MaybeTaggedVisitor::<T>(PhantomData))
    }
}

/// function to get the current cbor tag
///
/// The only place where it makes sense to call this function is within visit_newtype_struct of a serde visitor.
/// This is a low level API. In most cases it is preferable to use Tagged
pub fn current_cbor_tag() -> Option<u64> {
    get_tag()
}

#[cfg(feature = "tags")]
pub(crate) fn set_tag(value: Option<u64>) {
    CBOR_TAG.with(|f| *f.borrow_mut() = value);
}

#[cfg(feature = "tags")]
pub(crate) fn get_tag() -> Option<u64> {
    CBOR_TAG.with(|f| *f.borrow())
}

#[cfg(not(feature = "tags"))]
pub(crate) fn set_tag(_value: Option<u64>) {}

#[cfg(not(feature = "tags"))]
pub(crate) fn get_tag() -> Option<u64> {
    None
}

#[cfg(feature = "tags")]
use std::cell::RefCell;

#[cfg(feature = "tags")]
thread_local!(static CBOR_TAG: RefCell<Option<u64>> = RefCell::new(None));
