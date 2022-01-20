use crate::error::Error;
use crate::lib::*;
use serde::de::value::BorrowedStrDeserializer;
use serde::de::{
    self, Deserialize, DeserializeSeed, Deserializer, IntoDeserializer, MapAccess, Unexpected,
    Visitor,
};
use serde::forward_to_deserialize_any;
use serde::ser::{Serialize, SerializeStruct, Serializer};

/// Reference to a range of bytes encompassing a single valid JSON value in the
/// input data.
///
/// A `RawValue` can be used to defer parsing parts of a payload until later,
/// or to avoid parsing it at all in the case that part of the payload just
/// needs to be transferred verbatim into a different output object.
///
/// When serializing, a value of this type will retain its original formatting
/// and will not be minified or pretty-printed.
///
/// # Note
///
/// `RawValue` is only available if serde\_json is built with the `"raw_value"`
/// feature.
///
/// ```toml
/// [dependencies]
/// serde_json = { version = "1.0", features = ["raw_value"] }
/// ```
///
/// # Example
///
/// ```
/// use serde::{Deserialize, Serialize};
/// use serde_json::{Result, value::RawValue};
///
/// #[derive(Deserialize)]
/// struct Input<'a> {
///     code: u32,
///     #[serde(borrow)]
///     payload: &'a RawValue,
/// }
///
/// #[derive(Serialize)]
/// struct Output<'a> {
///     info: (u32, &'a RawValue),
/// }
///
/// // Efficiently rearrange JSON input containing separate "code" and "payload"
/// // keys into a single "info" key holding an array of code and payload.
/// //
/// // This could be done equivalently using serde_json::Value as the type for
/// // payload, but &RawValue will perform better because it does not require
/// // memory allocation. The correct range of bytes is borrowed from the input
/// // data and pasted verbatim into the output.
/// fn rearrange(input: &str) -> Result<String> {
///     let input: Input = serde_json::from_str(input)?;
///
///     let output = Output {
///         info: (input.code, input.payload),
///     };
///
///     serde_json::to_string(&output)
/// }
///
/// fn main() -> Result<()> {
///     let out = rearrange(r#" {"code": 200, "payload": {}} "#)?;
///
///     assert_eq!(out, r#"{"info":[200,{}]}"#);
///
///     Ok(())
/// }
/// ```
///
/// # Ownership
///
/// The typical usage of `RawValue` will be in the borrowed form:
///
/// ```
/// # use serde::Deserialize;
/// # use serde_json::value::RawValue;
/// #
/// #[derive(Deserialize)]
/// struct SomeStruct<'a> {
///     #[serde(borrow)]
///     raw_value: &'a RawValue,
/// }
/// ```
///
/// The borrowed form is suitable when deserializing through
/// [`serde_json::from_str`] and [`serde_json::from_slice`] which support
/// borrowing from the input data without memory allocation.
///
/// When deserializing through [`serde_json::from_reader`] you will need to use
/// the boxed form of `RawValue` instead. This is almost as efficient but
/// involves buffering the raw value from the I/O stream into memory.
///
/// [`serde_json::from_str`]: ../fn.from_str.html
/// [`serde_json::from_slice`]: ../fn.from_slice.html
/// [`serde_json::from_reader`]: ../fn.from_reader.html
///
/// ```
/// # use serde::Deserialize;
/// # use serde_json::value::RawValue;
/// #
/// #[derive(Deserialize)]
/// struct SomeStruct {
///     raw_value: Box<RawValue>,
/// }
/// ```
#[repr(C)]
pub struct RawValue {
    json: str,
}

impl RawValue {
    fn from_borrowed(json: &str) -> &Self {
        unsafe { mem::transmute::<&str, &RawValue>(json) }
    }

    fn from_owned(json: Box<str>) -> Box<Self> {
        unsafe { mem::transmute::<Box<str>, Box<RawValue>>(json) }
    }
}

impl Clone for Box<RawValue> {
    fn clone(&self) -> Self {
        (**self).to_owned()
    }
}

impl ToOwned for RawValue {
    type Owned = Box<RawValue>;

    fn to_owned(&self) -> Self::Owned {
        RawValue::from_owned(self.json.to_owned().into_boxed_str())
    }
}

impl Default for Box<RawValue> {
    fn default() -> Self {
        RawValue::from_borrowed("null").to_owned()
    }
}

impl Debug for RawValue {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter
            .debug_tuple("RawValue")
            .field(&format_args!("{}", &self.json))
            .finish()
    }
}

impl Display for RawValue {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str(&self.json)
    }
}

impl RawValue {
    /// Convert an owned `String` of JSON data to an owned `RawValue`.
    ///
    /// This function is equivalent to `serde_json::from_str::<Box<RawValue>>`
    /// except that we avoid an allocation and memcpy if both of the following
    /// are true:
    ///
    /// - the input has no leading or trailing whitespace, and
    /// - the input has capacity equal to its length.
    pub fn from_string(json: String) -> Result<Box<Self>, Error> {
        {
            let borrowed = crate::from_str::<&Self>(&json)?;
            if borrowed.json.len() < json.len() {
                return Ok(borrowed.to_owned());
            }
        }
        Ok(Self::from_owned(json.into_boxed_str()))
    }

    /// Access the JSON text underlying a raw value.
    ///
    /// # Example
    ///
    /// ```
    /// use serde::Deserialize;
    /// use serde_json::{Result, value::RawValue};
    ///
    /// #[derive(Deserialize)]
    /// struct Response<'a> {
    ///     code: u32,
    ///     #[serde(borrow)]
    ///     payload: &'a RawValue,
    /// }
    ///
    /// fn process(input: &str) -> Result<()> {
    ///     let response: Response = serde_json::from_str(input)?;
    ///
    ///     let payload = response.payload.get();
    ///     if payload.starts_with('{') {
    ///         // handle a payload which is a JSON map
    ///     } else {
    ///         // handle any other type
    ///     }
    ///
    ///     Ok(())
    /// }
    ///
    /// fn main() -> Result<()> {
    ///     process(r#" {"code": 200, "payload": {}} "#)?;
    ///     Ok(())
    /// }
    /// ```
    pub fn get(&self) -> &str {
        &self.json
    }
}

/// Convert a `T` into a boxed `RawValue`.
///
/// # Example
///
/// ```
/// // Upstream crate
/// # #[derive(Serialize)]
/// pub struct Thing {
///     foo: String,
///     bar: Option<String>,
///     extra_data: Box<RawValue>,
/// }
///
/// // Local crate
/// use serde::Serialize;
/// use serde_json::value::{to_raw_value, RawValue};
///
/// #[derive(Serialize)]
/// struct MyExtraData {
///     a: u32,
///     b: u32,
/// }
///
/// let my_thing = Thing {
///     foo: "FooVal".into(),
///     bar: None,
///     extra_data: to_raw_value(&MyExtraData { a: 1, b: 2 }).unwrap(),
/// };
/// # assert_eq!(
/// #     serde_json::to_value(my_thing).unwrap(),
/// #     serde_json::json!({
/// #         "foo": "FooVal",
/// #         "bar": null,
/// #         "extra_data": { "a": 1, "b": 2 }
/// #     })
/// # );
/// ```
///
/// # Errors
///
/// This conversion can fail if `T`'s implementation of `Serialize` decides to
/// fail, or if `T` contains a map with non-string keys.
///
/// ```
/// use std::collections::BTreeMap;
///
/// // The keys in this map are vectors, not strings.
/// let mut map = BTreeMap::new();
/// map.insert(vec![32, 64], "x86");
///
/// println!("{}", serde_json::value::to_raw_value(&map).unwrap_err());
/// ```
pub fn to_raw_value<T>(value: &T) -> Result<Box<RawValue>, Error>
where
    T: Serialize,
{
    let json_string = crate::to_string(value)?;
    Ok(RawValue::from_owned(json_string.into_boxed_str()))
}

pub const TOKEN: &str = "$serde_json::private::RawValue";

impl Serialize for RawValue {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let mut s = serializer.serialize_struct(TOKEN, 1)?;
        s.serialize_field(TOKEN, &self.json)?;
        s.end()
    }
}

impl<'de: 'a, 'a> Deserialize<'de> for &'a RawValue {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct ReferenceVisitor;

        impl<'de> Visitor<'de> for ReferenceVisitor {
            type Value = &'de RawValue;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                write!(formatter, "any valid JSON value")
            }

            fn visit_map<V>(self, mut visitor: V) -> Result<Self::Value, V::Error>
            where
                V: MapAccess<'de>,
            {
                let value = visitor.next_key::<RawKey>()?;
                if value.is_none() {
                    return Err(de::Error::invalid_type(Unexpected::Map, &self));
                }
                visitor.next_value_seed(ReferenceFromString)
            }
        }

        deserializer.deserialize_newtype_struct(TOKEN, ReferenceVisitor)
    }
}

impl<'de> Deserialize<'de> for Box<RawValue> {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct BoxedVisitor;

        impl<'de> Visitor<'de> for BoxedVisitor {
            type Value = Box<RawValue>;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                write!(formatter, "any valid JSON value")
            }

            fn visit_map<V>(self, mut visitor: V) -> Result<Self::Value, V::Error>
            where
                V: MapAccess<'de>,
            {
                let value = visitor.next_key::<RawKey>()?;
                if value.is_none() {
                    return Err(de::Error::invalid_type(Unexpected::Map, &self));
                }
                visitor.next_value_seed(BoxedFromString)
            }
        }

        deserializer.deserialize_newtype_struct(TOKEN, BoxedVisitor)
    }
}

struct RawKey;

impl<'de> Deserialize<'de> for RawKey {
    fn deserialize<D>(deserializer: D) -> Result<RawKey, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct FieldVisitor;

        impl<'de> Visitor<'de> for FieldVisitor {
            type Value = ();

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("raw value")
            }

            fn visit_str<E>(self, s: &str) -> Result<(), E>
            where
                E: de::Error,
            {
                if s == TOKEN {
                    Ok(())
                } else {
                    Err(de::Error::custom("unexpected raw value"))
                }
            }
        }

        deserializer.deserialize_identifier(FieldVisitor)?;
        Ok(RawKey)
    }
}

pub struct ReferenceFromString;

impl<'de> DeserializeSeed<'de> for ReferenceFromString {
    type Value = &'de RawValue;

    fn deserialize<D>(self, deserializer: D) -> Result<Self::Value, D::Error>
    where
        D: Deserializer<'de>,
    {
        deserializer.deserialize_str(self)
    }
}

impl<'de> Visitor<'de> for ReferenceFromString {
    type Value = &'de RawValue;

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("raw value")
    }

    fn visit_borrowed_str<E>(self, s: &'de str) -> Result<Self::Value, E>
    where
        E: de::Error,
    {
        Ok(RawValue::from_borrowed(s))
    }
}

pub struct BoxedFromString;

impl<'de> DeserializeSeed<'de> for BoxedFromString {
    type Value = Box<RawValue>;

    fn deserialize<D>(self, deserializer: D) -> Result<Self::Value, D::Error>
    where
        D: Deserializer<'de>,
    {
        deserializer.deserialize_str(self)
    }
}

impl<'de> Visitor<'de> for BoxedFromString {
    type Value = Box<RawValue>;

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("raw value")
    }

    fn visit_str<E>(self, s: &str) -> Result<Self::Value, E>
    where
        E: de::Error,
    {
        self.visit_string(s.to_owned())
    }

    fn visit_string<E>(self, s: String) -> Result<Self::Value, E>
    where
        E: de::Error,
    {
        Ok(RawValue::from_owned(s.into_boxed_str()))
    }
}

struct RawKeyDeserializer;

impl<'de> Deserializer<'de> for RawKeyDeserializer {
    type Error = Error;

    fn deserialize_any<V>(self, visitor: V) -> Result<V::Value, Error>
    where
        V: de::Visitor<'de>,
    {
        visitor.visit_borrowed_str(TOKEN)
    }

    forward_to_deserialize_any! {
        bool u8 u16 u32 u64 u128 i8 i16 i32 i64 i128 f32 f64 char str string seq
        bytes byte_buf map struct option unit newtype_struct ignored_any
        unit_struct tuple_struct tuple enum identifier
    }
}

pub struct OwnedRawDeserializer {
    pub raw_value: Option<String>,
}

impl<'de> MapAccess<'de> for OwnedRawDeserializer {
    type Error = Error;

    fn next_key_seed<K>(&mut self, seed: K) -> Result<Option<K::Value>, Error>
    where
        K: de::DeserializeSeed<'de>,
    {
        if self.raw_value.is_none() {
            return Ok(None);
        }
        seed.deserialize(RawKeyDeserializer).map(Some)
    }

    fn next_value_seed<V>(&mut self, seed: V) -> Result<V::Value, Error>
    where
        V: de::DeserializeSeed<'de>,
    {
        seed.deserialize(self.raw_value.take().unwrap().into_deserializer())
    }
}

pub struct BorrowedRawDeserializer<'de> {
    pub raw_value: Option<&'de str>,
}

impl<'de> MapAccess<'de> for BorrowedRawDeserializer<'de> {
    type Error = Error;

    fn next_key_seed<K>(&mut self, seed: K) -> Result<Option<K::Value>, Error>
    where
        K: de::DeserializeSeed<'de>,
    {
        if self.raw_value.is_none() {
            return Ok(None);
        }
        seed.deserialize(RawKeyDeserializer).map(Some)
    }

    fn next_value_seed<V>(&mut self, seed: V) -> Result<V::Value, Error>
    where
        V: de::DeserializeSeed<'de>,
    {
        seed.deserialize(BorrowedStrDeserializer::new(self.raw_value.take().unwrap()))
    }
}
