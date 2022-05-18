//! De/Serialization for Rust's builtin and std types

use serde::{
    de::{Deserialize, DeserializeOwned, Deserializer, Error, MapAccess, SeqAccess, Visitor},
    ser::{Serialize, SerializeMap, SerializeSeq, Serializer},
};
use std::{
    cmp::Eq,
    collections::{BTreeMap, HashMap},
    fmt::{self, Display},
    hash::{BuildHasher, Hash},
    iter::FromIterator,
    marker::PhantomData,
    str::FromStr,
};
use Separator;

/// De/Serialize using [`Display`] and [`FromStr`] implementation
///
/// This allows to deserialize a string as a number.
/// It can be very useful for serialization formats like JSON, which do not support integer
/// numbers and have to resort to strings to represent them.
///
/// # Examples
///
/// ```
/// # extern crate serde;
/// # extern crate serde_derive;
/// # extern crate serde_json;
/// # extern crate serde_with;
/// #
/// # use serde_derive::{Deserialize, Serialize};
/// # use std::net::Ipv4Addr;
/// #
/// #[derive(Deserialize, Serialize)]
/// struct A {
///     #[serde(with = "serde_with::rust::display_fromstr")]
///     address: Ipv4Addr,
///     #[serde(with = "serde_with::rust::display_fromstr")]
///     b: bool,
/// }
///
/// let v: A = serde_json::from_str(r#"{
///     "address": "192.168.2.1",
///     "b": "true"
/// }"#).unwrap();
/// assert_eq!(Ipv4Addr::new(192, 168, 2, 1), v.address);
/// assert!(v.b);
///
/// let x = A {
///     address: Ipv4Addr::new(127, 53, 0, 1),
///     b: false,
/// };
/// assert_eq!(r#"{"address":"127.53.0.1","b":"false"}"#, serde_json::to_string(&x).unwrap());
/// ```
pub mod display_fromstr {
    use super::*;
    use std::str::FromStr;

    /// Deserialize T using [FromStr]
    pub fn deserialize<'de, D, T>(deserializer: D) -> Result<T, D::Error>
    where
        D: Deserializer<'de>,
        T: FromStr,
        T::Err: Display,
    {
        struct Helper<S>(PhantomData<S>);

        impl<'de, S> Visitor<'de> for Helper<S>
        where
            S: FromStr,
            <S as FromStr>::Err: Display,
        {
            type Value = S;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                write!(formatter, "valid json object")
            }

            fn visit_str<E>(self, value: &str) -> Result<Self::Value, E>
            where
                E: Error,
            {
                value.parse::<Self::Value>().map_err(Error::custom)
            }
        }

        deserializer.deserialize_str(Helper(PhantomData))
    }

    /// Serialize T using [Display]
    pub fn serialize<T, S>(value: &T, serializer: S) -> Result<S::Ok, S::Error>
    where
        T: Display,
        S: Serializer,
    {
        serializer.serialize_str(&*value.to_string())
    }
}

/// De/Serialize sequences using [`FromIterator`] and [`IntoIterator`] implementation for it and [`Display`] and [`FromStr`] implementation for each element
///
/// This allows to serialize and deserialize collections with elements which can be represented as strings.
///
/// # Examples
///
/// ```
/// # extern crate serde;
/// # extern crate serde_derive;
/// # extern crate serde_json;
/// # extern crate serde_with;
/// #
/// # use serde_derive::{Deserialize, Serialize};
/// #
/// use std::collections::BTreeSet;
/// use std::net::Ipv4Addr;
///
/// #[derive(Deserialize, Serialize)]
/// struct A {
///     #[serde(with = "serde_with::rust::seq_display_fromstr")]
///     addresses: BTreeSet<Ipv4Addr>,
///     #[serde(with = "serde_with::rust::seq_display_fromstr")]
///     bs: Vec<bool>,
/// }
///
/// let v: A = serde_json::from_str(r#"{
///     "addresses": ["192.168.2.1", "192.168.2.2", "192.168.1.1", "192.168.2.2"],
///     "bs": ["true", "false"]
/// }"#).unwrap();
/// assert_eq!(v.addresses.len(), 3);
/// assert!(v.addresses.contains(&Ipv4Addr::new(192, 168, 2, 1)));
/// assert!(v.addresses.contains(&Ipv4Addr::new(192, 168, 2, 2)));
/// assert!(!v.addresses.contains(&Ipv4Addr::new(192, 168, 1, 2)));
/// assert_eq!(v.bs.len(), 2);
/// assert!(v.bs[0]);
/// assert!(!v.bs[1]);
///
/// let x = A {
///     addresses: vec![
///         Ipv4Addr::new(127, 53, 0, 1),
///         Ipv4Addr::new(127, 53, 1, 1),
///         Ipv4Addr::new(127, 53, 0, 2)
///     ].into_iter().collect(),
///     bs: vec![false, true],
/// };
/// assert_eq!(r#"{"addresses":["127.53.0.1","127.53.0.2","127.53.1.1"],"bs":["false","true"]}"#, serde_json::to_string(&x).unwrap());
/// ```
pub mod seq_display_fromstr {
    use serde::{
        de::{Deserializer, Error, SeqAccess, Visitor},
        ser::{SerializeSeq, Serializer},
    };
    use std::{
        fmt::{self, Display},
        iter::{FromIterator, IntoIterator},
        marker::PhantomData,
        str::FromStr,
    };

    /// Deserialize collection T using [FromIterator] and [FromStr] for each element
    pub fn deserialize<'de, D, T, I>(deserializer: D) -> Result<T, D::Error>
    where
        D: Deserializer<'de>,
        T: FromIterator<I> + Sized,
        I: FromStr,
        I::Err: Display,
    {
        struct Helper<S>(PhantomData<S>);

        impl<'de, S> Visitor<'de> for Helper<S>
        where
            S: FromStr,
            <S as FromStr>::Err: Display,
        {
            type Value = Vec<S>;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                write!(formatter, "a sequence")
            }

            fn visit_seq<A>(self, mut access: A) -> Result<Self::Value, A::Error>
            where
                A: SeqAccess<'de>,
            {
                let mut values = access
                    .size_hint()
                    .map(Self::Value::with_capacity)
                    .unwrap_or_else(Self::Value::new);

                while let Some(value) = access.next_element::<&str>()? {
                    values.push(value.parse::<S>().map_err(Error::custom)?);
                }

                Ok(values)
            }
        }

        deserializer
            .deserialize_seq(Helper(PhantomData))
            .map(T::from_iter)
    }

    /// Serialize collection T using [IntoIterator] and [Display] for each element
    pub fn serialize<S, T, I>(value: &T, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
        for<'a> &'a T: IntoIterator<Item = &'a I>,
        I: Display,
    {
        let iter = value.into_iter();
        let (_, to) = iter.size_hint();
        let mut seq = serializer.serialize_seq(to)?;
        for item in iter {
            seq.serialize_element(&item.to_string())?;
        }
        seq.end()
    }
}

/// De/Serialize a delimited collection using [`Display`] and [`FromStr`] implementation
///
/// You can define an arbitrary separator, by specifying a type which implements [`Separator`].
/// Some common ones, like space and comma are already predefined and you can find them [here][Separator].
///
/// An empty string deserializes as an empty collection.
///
/// # Examples
///
/// ```
/// # extern crate serde;
/// # extern crate serde_derive;
/// # extern crate serde_json;
/// # extern crate serde_with;
/// #
/// # use serde_derive::{Deserialize, Serialize};
/// #
/// use serde_with::{CommaSeparator, SpaceSeparator};
/// use std::collections::BTreeSet;
///
/// #[derive(Deserialize, Serialize)]
/// struct A {
///     #[serde(with = "serde_with::rust::StringWithSeparator::<SpaceSeparator>")]
///     tags: Vec<String>,
///     #[serde(with = "serde_with::rust::StringWithSeparator::<CommaSeparator>")]
///     more_tags: BTreeSet<String>,
/// }
///
/// let v: A = serde_json::from_str(r##"{
///     "tags": "#hello #world",
///     "more_tags": "foo,bar,bar"
/// }"##).unwrap();
/// assert_eq!(vec!["#hello", "#world"], v.tags);
/// assert_eq!(2, v.more_tags.len());
///
/// let x = A {
///     tags: vec!["1".to_string(), "2".to_string(), "3".to_string()],
///     more_tags: BTreeSet::new(),
/// };
/// assert_eq!(r#"{"tags":"1 2 3","more_tags":""}"#, serde_json::to_string(&x).unwrap());
/// ```
#[derive(Copy, Clone, Eq, PartialEq, Ord, PartialOrd, Hash, Debug, Default)]
pub struct StringWithSeparator<Sep>(PhantomData<Sep>);

impl<Sep> StringWithSeparator<Sep>
where
    Sep: Separator,
{
    /// Serialize collection into a string with separator symbol
    pub fn serialize<S, T, V>(values: T, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
        T: IntoIterator<Item = V>,
        V: Display,
    {
        let mut s = String::new();
        for v in values {
            s.push_str(&*v.to_string());
            s.push_str(Sep::separator());
        }
        serializer.serialize_str(if !s.is_empty() {
            // remove trailing separator if present
            &s[..s.len() - Sep::separator().len()]
        } else {
            &s[..]
        })
    }

    /// Deserialize a collection from a string with separator symbol
    pub fn deserialize<'de, D, T, V>(deserializer: D) -> Result<T, D::Error>
    where
        D: Deserializer<'de>,
        T: FromIterator<V>,
        V: FromStr,
        V::Err: Display,
    {
        let s = String::deserialize(deserializer)?;
        if s.is_empty() {
            Ok(None.into_iter().collect())
        } else {
            s.split(Sep::separator())
                .map(FromStr::from_str)
                .collect::<Result<_, _>>()
                .map_err(Error::custom)
        }
    }
}

/// Makes a distinction between a missing, unset, or existing value
///
/// Some serialization formats make a distinction between missing fields, fields with a `null`
/// value, and existing values. One such format is JSON. By default it is not easily possible to
/// differentiate between a missing value and a field which is `null`, as they deserialize to the
/// same value. This helper changes it, by using an `Option<Option<T>>` to deserialize into.
///
/// * `None`: Represents a missing value.
/// * `Some(None)`: Represents a `null` value.
/// * `Some(Some(value))`: Represents an existing value.
///
/// # Examples
///
/// ```rust
/// # extern crate serde;
/// # extern crate serde_derive;
/// # extern crate serde_json;
/// # extern crate serde_with;
/// #
/// # use serde_derive::{Deserialize, Serialize};
/// #
/// # #[derive(Debug, PartialEq, Eq)]
/// #[derive(Deserialize, Serialize)]
/// struct Doc {
///     #[serde(
///         default,                                    // <- important for deserialization
///         skip_serializing_if = "Option::is_none",    // <- important for serialization
///         with = "::serde_with::rust::double_option",
///     )]
///     a: Option<Option<u8>>,
/// }
/// // Missing Value
/// let s = r#"{}"#;
/// assert_eq!(Doc {a: None}, serde_json::from_str(s).unwrap());
/// assert_eq!(s, serde_json::to_string(&Doc {a: None}).unwrap());
///
/// // Unset Value
/// let s = r#"{"a":null}"#;
/// assert_eq!(Doc {a: Some(None)}, serde_json::from_str(s).unwrap());
/// assert_eq!(s, serde_json::to_string(&Doc {a: Some(None)}).unwrap());
///
/// // Existing Value
/// let s = r#"{"a":5}"#;
/// assert_eq!(Doc {a: Some(Some(5))}, serde_json::from_str(s).unwrap());
/// assert_eq!(s, serde_json::to_string(&Doc {a: Some(Some(5))}).unwrap());
/// ```
#[cfg_attr(feature = "cargo-clippy", allow(option_option))]
pub mod double_option {
    use super::*;

    /// Deserialize potentially non-existing optional value
    pub fn deserialize<'de, T, D>(deserializer: D) -> Result<Option<Option<T>>, D::Error>
    where
        T: Deserialize<'de>,
        D: Deserializer<'de>,
    {
        Deserialize::deserialize(deserializer).map(Some)
    }

    /// Serialize optional value
    pub fn serialize<S, T>(values: &Option<Option<T>>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
        T: Serialize,
    {
        match values {
            None => serializer.serialize_unit(),
            Some(None) => serializer.serialize_none(),
            Some(Some(v)) => serializer.serialize_some(&v),
        }
    }
}

/// Serialize inner value if [`Some`]`(T)`. If [`None`], serialize the unit struct `()`.
///
/// When used in conjunction with `skip_serializing_if = "Option::is_none"` and
/// `default`, you can build an optional value by skipping if it is [`None`], or serializing its
/// inner value if [`Some`]`(T)`.
///
/// Not all serialization formats easily support optional values.
/// While JSON uses the [`Option`] type to represent optional values and only serializes the inner
/// part of the [`Some`]`()`, other serialization formats, such as [RON][], choose to serialize the
/// [`Some`] around a value.
/// This helper helps building a truly optional value for such serializers.
///
/// [RON]: https://github.com/ron-rs/ron
///
/// # Example
///
/// ```rust
/// # extern crate serde;
/// # extern crate serde_derive;
/// # extern crate serde_json;
/// # extern crate serde_with;
/// # extern crate ron;
/// #
/// # use serde_derive::{Deserialize, Serialize};
/// #
/// # #[derive(Debug, Eq, PartialEq)]
/// #[derive(Deserialize, Serialize)]
/// struct Doc {
///     mandatory: usize,
///     #[serde(
///         default,                                    // <- important for deserialization
///         skip_serializing_if = "Option::is_none",    // <- important for serialization
///         with = "::serde_with::rust::unwrap_or_skip",
///     )]
///     optional: Option<usize>,
/// }
///
/// // Transparently add/remove Some() wrapper
/// # let pretty_config = ron::ser::PrettyConfig {
/// #     new_line: "\n".into(),
/// #     ..Default::default()
/// # };
/// let s = r#"(
///     mandatory: 1,
///     optional: 2,
/// )"#;
/// let v = Doc {
///     mandatory: 1,
///     optional: Some(2),
/// };
/// assert_eq!(v, ron::de::from_str(s).unwrap());
/// assert_eq!(s, ron::ser::to_string_pretty(&v, pretty_config).unwrap());
///
/// // Missing values are deserialized as `None`
/// // while `None` values are skipped during serialization.
/// # let pretty_config = ron::ser::PrettyConfig {
/// #     new_line: "\n".into(),
/// #     ..Default::default()
/// # };
/// let s = r#"(
///     mandatory: 1,
/// )"#;
/// let v = Doc {
///     mandatory: 1,
///     optional: None,
/// };
/// assert_eq!(v, ron::de::from_str(s).unwrap());
/// assert_eq!(s, ron::ser::to_string_pretty(&v, pretty_config).unwrap());
/// ```
pub mod unwrap_or_skip {
    use super::*;

    /// Deserialize value wrapped in Some(T)
    pub fn deserialize<'de, D, T>(deserializer: D) -> Result<Option<T>, D::Error>
    where
        D: Deserializer<'de>,
        T: DeserializeOwned,
    {
        T::deserialize(deserializer).map(Some)
    }

    /// Serialize value if Some(T), unit struct if None
    pub fn serialize<T, S>(option: &Option<T>, serializer: S) -> Result<S::Ok, S::Error>
    where
        T: Serialize,
        S: Serializer,
    {
        if let Some(value) = option {
            value.serialize(serializer)
        } else {
            ().serialize(serializer)
        }
    }
}

/// Ensure no duplicate values exist in a set.
///
/// By default serde has a last-value-wins implementation, if duplicate values for a set exist.
/// Sometimes it is desirable to know when such an event happens, as the first value is overwritten
/// and it can indicate an error in the serialized data.
///
/// This helper returns an error if two identical values exist in a set.
///
/// The implementation supports both the [`HashSet`] and the [`BTreeSet`] from the standard library.
///
/// [`HashSet`]: std::collections::HashSet
/// [`BTreeSet`]: std::collections::HashSet
///
/// # Example
///
/// ```rust
/// # extern crate serde;
/// # extern crate serde_derive;
/// # extern crate serde_json;
/// # extern crate serde_with;
/// #
/// # use std::{collections::HashSet, iter::FromIterator};
/// # use serde_derive::{Deserialize, Serialize};
/// #
/// # #[derive(Debug, Eq, PartialEq)]
/// #[derive(Deserialize)]
/// struct Doc {
///     #[serde(with = "::serde_with::rust::sets_duplicate_value_is_error")]
///     set: HashSet<usize>,
/// }
///
/// // Sets are serialized normally,
/// let s = r#"{"set": [1, 2, 3, 4]}"#;
/// let v = Doc {
///     set: HashSet::from_iter(vec![1, 2, 3, 4]),
/// };
/// assert_eq!(v, serde_json::from_str(s).unwrap());
///
/// // but create an error if duplicate values, like the `1`, exist.
/// let s = r#"{"set": [1, 2, 3, 4, 1]}"#;
/// let res: Result<Doc, _> = serde_json::from_str(s);
/// assert!(res.is_err());
/// ```
pub mod sets_duplicate_value_is_error {
    use super::*;
    use duplicate_key_impls::PreventDuplicateInsertsSet;

    /// Deserialize a set and return an error on duplicate values
    pub fn deserialize<'de, D, T, V>(deserializer: D) -> Result<T, D::Error>
    where
        T: PreventDuplicateInsertsSet<V>,
        V: Deserialize<'de>,
        D: Deserializer<'de>,
    {
        struct SeqVisitor<T, V> {
            marker: PhantomData<T>,
            set_item_type: PhantomData<V>,
        };

        impl<'de, T, V> Visitor<'de> for SeqVisitor<T, V>
        where
            T: PreventDuplicateInsertsSet<V>,
            V: Deserialize<'de>,
        {
            type Value = T;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("a sequence")
            }

            #[inline]
            fn visit_seq<A>(self, mut access: A) -> Result<Self::Value, A::Error>
            where
                A: SeqAccess<'de>,
            {
                let mut values = Self::Value::new(access.size_hint());

                while let Some(value) = access.next_element()? {
                    if !values.insert(value) {
                        return Err(Error::custom("invalid entry: found duplicate value"));
                    };
                }

                Ok(values)
            }
        }

        let visitor = SeqVisitor {
            marker: PhantomData,
            set_item_type: PhantomData,
        };
        deserializer.deserialize_seq(visitor)
    }
}

/// Ensure no duplicate keys exist in a map.
///
/// By default serde has a last-value-wins implementation, if duplicate keys for a map exist.
/// Sometimes it is desirable to know when such an event happens, as the first value is overwritten
/// and it can indicate an error in the serialized data.
///
/// This helper returns an error if two identical keys exist in a map.
///
/// The implementation supports both the [`HashMap`] and the [`BTreeMap`] from the standard library.
///
/// [`HashMap`]: std::collections::HashMap
/// [`BTreeMap`]: std::collections::HashMap
///
/// # Example
///
/// ```rust
/// # extern crate serde;
/// # extern crate serde_derive;
/// # extern crate serde_json;
/// # extern crate serde_with;
/// #
/// # use serde_derive::{Deserialize, Serialize};
/// # use std::collections::HashMap;
/// #
/// # #[derive(Debug, Eq, PartialEq)]
/// #[derive(Deserialize)]
/// struct Doc {
///     #[serde(with = "::serde_with::rust::maps_duplicate_key_is_error")]
///     map: HashMap<usize, usize>,
/// }
///
/// // Maps are serialized normally,
/// let s = r#"{"map": {"1": 1, "2": 2, "3": 3}}"#;
/// let mut v = Doc {
///     map: HashMap::new(),
/// };
/// v.map.insert(1, 1);
/// v.map.insert(2, 2);
/// v.map.insert(3, 3);
/// assert_eq!(v, serde_json::from_str(s).unwrap());
///
/// // but create an error if duplicate keys, like the `1`, exist.
/// let s = r#"{"map": {"1": 1, "2": 2, "1": 3}}"#;
/// let res: Result<Doc, _> = serde_json::from_str(s);
/// assert!(res.is_err());
/// ```
pub mod maps_duplicate_key_is_error {
    use super::*;
    use duplicate_key_impls::PreventDuplicateInsertsMap;

    /// Deserialize a map and return an error on duplicate keys
    pub fn deserialize<'de, D, T, K, V>(deserializer: D) -> Result<T, D::Error>
    where
        T: PreventDuplicateInsertsMap<K, V>,
        K: Deserialize<'de>,
        V: Deserialize<'de>,
        D: Deserializer<'de>,
    {
        struct MapVisitor<T, K, V> {
            marker: PhantomData<T>,
            map_key_type: PhantomData<K>,
            map_value_type: PhantomData<V>,
        };

        impl<'de, T, K, V> Visitor<'de> for MapVisitor<T, K, V>
        where
            T: PreventDuplicateInsertsMap<K, V>,
            K: Deserialize<'de>,
            V: Deserialize<'de>,
        {
            type Value = T;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("a map")
            }

            #[inline]
            fn visit_map<A>(self, mut access: A) -> Result<Self::Value, A::Error>
            where
                A: MapAccess<'de>,
            {
                let mut values = Self::Value::new(access.size_hint());

                while let Some((key, value)) = access.next_entry()? {
                    if !values.insert(key, value) {
                        return Err(Error::custom("invalid entry: found duplicate key"));
                    };
                }

                Ok(values)
            }
        }

        let visitor = MapVisitor {
            marker: PhantomData,
            map_key_type: PhantomData,
            map_value_type: PhantomData,
        };
        deserializer.deserialize_map(visitor)
    }
}

/// Ensure that the first value is taken, if duplicate values exist
///
/// By default serde has a last-value-wins implementation, if duplicate keys for a set exist.
/// Sometimes the opposite strategy is desired. This helper implements a first-value-wins strategy.
///
/// The implementation supports both the [`HashSet`] and the [`BTreeSet`] from the standard library.
///
/// [`HashSet`]: std::collections::HashSet
/// [`BTreeSet`]: std::collections::HashSet
pub mod sets_first_value_wins {
    use super::*;
    use duplicate_key_impls::DuplicateInsertsFirstWinsSet;

    /// Deserialize a set and return an error on duplicate values
    pub fn deserialize<'de, D, T, V>(deserializer: D) -> Result<T, D::Error>
    where
        T: DuplicateInsertsFirstWinsSet<V>,
        V: Deserialize<'de>,
        D: Deserializer<'de>,
    {
        struct SeqVisitor<T, V> {
            marker: PhantomData<T>,
            set_item_type: PhantomData<V>,
        };

        impl<'de, T, V> Visitor<'de> for SeqVisitor<T, V>
        where
            T: DuplicateInsertsFirstWinsSet<V>,
            V: Deserialize<'de>,
        {
            type Value = T;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("a sequence")
            }

            #[inline]
            fn visit_seq<A>(self, mut access: A) -> Result<Self::Value, A::Error>
            where
                A: SeqAccess<'de>,
            {
                let mut values = Self::Value::new(access.size_hint());

                while let Some(value) = access.next_element()? {
                    values.insert(value);
                }

                Ok(values)
            }
        }

        let visitor = SeqVisitor {
            marker: PhantomData,
            set_item_type: PhantomData,
        };
        deserializer.deserialize_seq(visitor)
    }
}

/// Ensure that the first key is taken, if duplicate keys exist
///
/// By default serde has a last-key-wins implementation, if duplicate keys for a map exist.
/// Sometimes the opposite strategy is desired. This helper implements a first-key-wins strategy.
///
/// The implementation supports both the [`HashMap`] and the [`BTreeMap`] from the standard library.
///
/// [`HashMap`]: std::collections::HashMap
/// [`BTreeMap`]: std::collections::HashMap
///
/// # Example
///
/// ```rust
/// # extern crate serde;
/// # extern crate serde_derive;
/// # extern crate serde_json;
/// # extern crate serde_with;
/// #
/// # use serde_derive::{Deserialize, Serialize};
/// # use std::collections::HashMap;
/// #
/// # #[derive(Debug, Eq, PartialEq)]
/// #[derive(Deserialize)]
/// struct Doc {
///     #[serde(with = "::serde_with::rust::maps_first_key_wins")]
///     map: HashMap<usize, usize>,
/// }
///
/// // Maps are serialized normally,
/// let s = r#"{"map": {"1": 1, "2": 2, "3": 3}}"#;
/// let mut v = Doc {
///     map: HashMap::new(),
/// };
/// v.map.insert(1, 1);
/// v.map.insert(2, 2);
/// v.map.insert(3, 3);
/// assert_eq!(v, serde_json::from_str(s).unwrap());
///
/// // but create an error if duplicate keys, like the `1`, exist.
/// let s = r#"{"map": {"1": 1, "2": 2, "1": 3}}"#;
/// let mut v = Doc {
///     map: HashMap::new(),
/// };
/// v.map.insert(1, 1);
/// v.map.insert(2, 2);
/// assert_eq!(v, serde_json::from_str(s).unwrap());
/// ```
pub mod maps_first_key_wins {
    use super::*;
    use duplicate_key_impls::DuplicateInsertsFirstWinsMap;

    /// Deserialize a map and return an error on duplicate keys
    pub fn deserialize<'de, D, T, K, V>(deserializer: D) -> Result<T, D::Error>
    where
        T: DuplicateInsertsFirstWinsMap<K, V>,
        K: Deserialize<'de>,
        V: Deserialize<'de>,
        D: Deserializer<'de>,
    {
        struct MapVisitor<T, K, V> {
            marker: PhantomData<T>,
            map_key_type: PhantomData<K>,
            map_value_type: PhantomData<V>,
        };

        impl<'de, T, K, V> Visitor<'de> for MapVisitor<T, K, V>
        where
            T: DuplicateInsertsFirstWinsMap<K, V>,
            K: Deserialize<'de>,
            V: Deserialize<'de>,
        {
            type Value = T;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("a map")
            }

            #[inline]
            fn visit_map<A>(self, mut access: A) -> Result<Self::Value, A::Error>
            where
                A: MapAccess<'de>,
            {
                let mut values = Self::Value::new(access.size_hint());

                while let Some((key, value)) = access.next_entry()? {
                    values.insert(key, value);
                }

                Ok(values)
            }
        }

        let visitor = MapVisitor {
            marker: PhantomData,
            map_key_type: PhantomData,
            map_value_type: PhantomData,
        };
        deserializer.deserialize_map(visitor)
    }
}

/// De/Serialize a [`Option`]`<String>` type while transforming the empty string to [`None`]
///
/// Convert an [`Option`]`<T>` from/to string using [`FromStr`] and [`AsRef`]`<str>` implementations.
/// An empty string is deserialized as [`None`] and a [`None`] vice versa.
///
/// # Examples
///
/// ```
/// # extern crate serde;
/// # extern crate serde_derive;
/// # extern crate serde_json;
/// # extern crate serde_with;
/// #
/// # use serde_derive::{Deserialize, Serialize};
/// #
/// #[derive(Deserialize, Serialize)]
/// struct A {
///     #[serde(with = "serde_with::rust::string_empty_as_none")]
///     tags: Option<String>,
/// }
///
/// let v: A = serde_json::from_str(r##"{
///     "tags": ""
/// }"##).unwrap();
/// assert!(v.tags.is_none());
///
/// let v: A = serde_json::from_str(r##"{
///     "tags": "Hi"
/// }"##).unwrap();
/// assert_eq!(Some("Hi".to_string()), v.tags);
///
/// let x = A {
///     tags: Some("This is text".to_string()),
/// };
/// assert_eq!(r#"{"tags":"This is text"}"#, serde_json::to_string(&x).unwrap());
///
/// let x = A {
///     tags: None,
/// };
/// assert_eq!(r#"{"tags":""}"#, serde_json::to_string(&x).unwrap());
/// ```
pub mod string_empty_as_none {
    use super::*;

    /// Deserialize an `Option<T>` from a string using `FromStr`
    pub fn deserialize<'de, D, S>(deserializer: D) -> Result<Option<S>, D::Error>
    where
        D: Deserializer<'de>,
        S: FromStr,
        S::Err: Display,
    {
        struct OptionStringEmptyNone<S>(PhantomData<S>);
        impl<'de, S> Visitor<'de> for OptionStringEmptyNone<S>
        where
            S: FromStr,
            S::Err: Display,
        {
            type Value = Option<S>;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("any string")
            }

            fn visit_str<E>(self, value: &str) -> Result<Self::Value, E>
            where
                E: Error,
            {
                match value {
                    "" => Ok(None),
                    v => S::from_str(v).map(Some).map_err(Error::custom),
                }
            }

            fn visit_string<E>(self, value: String) -> Result<Self::Value, E>
            where
                E: Error,
            {
                match &*value {
                    "" => Ok(None),
                    v => S::from_str(v).map(Some).map_err(Error::custom),
                }
            }

            // handles the `null` case
            fn visit_unit<E>(self) -> Result<Self::Value, E>
            where
                E: Error,
            {
                Ok(None)
            }
        }

        deserializer.deserialize_any(OptionStringEmptyNone(PhantomData))
    }

    /// Serialize a string from `Option<T>` using `AsRef<str>` or using the empty string if `None`.
    pub fn serialize<T, S>(option: &Option<T>, serializer: S) -> Result<S::Ok, S::Error>
    where
        T: AsRef<str>,
        S: Serializer,
    {
        if let Some(value) = option {
            value.as_ref().serialize(serializer)
        } else {
            "".serialize(serializer)
        }
    }
}

/// De/Serialize a [`HashMap`] into a list of tuples
///
/// Some formats, like JSON, have limitations on the type of keys for maps.
/// In case of JSON, keys are restricted to strings.
/// Rust features more powerfull keys, for example tuple, which can not be serialized to JSON.
///
/// This helper serializes the [`HashMap`] into a list of tuples, which does not have the same type restrictions.
///
/// If you need to de/serialize a [`BTreeMap`] then use [`btreemap_as_tuple_list`].
///
/// # Examples
///
/// ```
/// # extern crate serde;
/// # extern crate serde_derive;
/// # extern crate serde_json;
/// # extern crate serde_with;
/// #
/// # use serde_derive::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use std::collections::HashMap;
/// #
/// #[derive(Deserialize, Serialize)]
/// struct A {
///     #[serde(with = "serde_with::rust::hashmap_as_tuple_list")]
///     s: HashMap<(String, u32), u32>,
/// }
///
/// let v: A = serde_json::from_value(json!({
///     "s": [
///         [["Hello", 123], 0],
///         [["World", 456], 1]
///     ]
/// })).unwrap();
///
/// assert_eq!(2, v.s.len());
/// assert_eq!(1, v.s[&("World".to_string(), 456)]);
/// ```
///
/// The helper is generic over the hasher type of the [`HashMap`] and works with different variants, such as `FnvHashMap`.
///
/// ```
/// # extern crate fnv;
/// # extern crate serde;
/// # extern crate serde_derive;
/// # extern crate serde_json;
/// # extern crate serde_with;
/// #
/// # use serde_derive::{Deserialize, Serialize};
/// # use serde_json::json;
/// #
/// use fnv::FnvHashMap;
///
/// #[derive(Deserialize, Serialize)]
/// struct A {
///     #[serde(with = "serde_with::rust::hashmap_as_tuple_list")]
///     s: FnvHashMap<u32, bool>,
/// }
///
/// let v: A = serde_json::from_value(json!({
///     "s": [
///         [0, false],
///         [1, true]
///     ]
/// })).unwrap();
///
/// assert_eq!(2, v.s.len());
/// assert_eq!(true, v.s[&1]);
/// ```
pub mod hashmap_as_tuple_list {
    use super::{SerializeSeq, *}; // Needed to remove the unused import warning in the parent scope

    /// Serialize the [`HashMap`] as a list of tuples
    pub fn serialize<K, V, S, BH>(map: &HashMap<K, V, BH>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
        K: Eq + Hash + Serialize,
        V: Serialize,
        BH: BuildHasher,
    {
        let mut seq = serializer.serialize_seq(Some(map.len()))?;
        for item in map.iter() {
            seq.serialize_element(&item)?;
        }
        seq.end()
    }

    /// Deserialize a [`HashMap`] from a list of tuples
    pub fn deserialize<'de, K, V, BH, D>(deserializer: D) -> Result<HashMap<K, V, BH>, D::Error>
    where
        D: Deserializer<'de>,
        K: Eq + Hash + Deserialize<'de>,
        V: Deserialize<'de>,
        BH: BuildHasher + Default,
    {
        deserializer.deserialize_seq(HashMapVisitor(PhantomData))
    }

    #[cfg_attr(feature = "cargo-clippy", allow(type_complexity))]
    struct HashMapVisitor<K, V, BH>(PhantomData<fn() -> HashMap<K, V, BH>>);

    impl<'de, K, V, BH> Visitor<'de> for HashMapVisitor<K, V, BH>
    where
        K: Deserialize<'de> + Eq + Hash,
        V: Deserialize<'de>,
        BH: BuildHasher + Default,
    {
        type Value = HashMap<K, V, BH>;

        fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
            formatter.write_str("a list of key-value pairs")
        }

        fn visit_seq<A>(self, mut seq: A) -> Result<Self::Value, A::Error>
        where
            A: SeqAccess<'de>,
        {
            let mut map =
                HashMap::with_capacity_and_hasher(seq.size_hint().unwrap_or(0), BH::default());
            while let Some((key, value)) = seq.next_element()? {
                map.insert(key, value);
            }
            Ok(map)
        }
    }
}

/// De/Serialize a [`BTreeMap`] into a list of tuples
///
/// Some formats, like JSON, have limitations on the type of keys for maps.
/// In case of JSON, keys are restricted to strings.
/// Rust features more powerfull keys, for example tuple, which can not be serialized to JSON.
///
/// This helper serializes the [`BTreeMap`] into a list of tuples, which does not have the same type restrictions.
///
/// If you need to de/serialize a [`HashMap`] then use [`hashmap_as_tuple_list`].
///
/// # Examples
///
/// ```
/// # extern crate serde;
/// # extern crate serde_derive;
/// # extern crate serde_json;
/// # extern crate serde_with;
/// #
/// # use serde_derive::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use std::collections::BTreeMap;
/// #
/// #[derive(Deserialize, Serialize)]
/// struct A {
///     #[serde(with = "serde_with::rust::btreemap_as_tuple_list")]
///     s: BTreeMap<(String, u32), u32>,
/// }
///
/// let v: A = serde_json::from_value(json!({
///     "s": [
///         [["Hello", 123], 0],
///         [["World", 456], 1]
///     ]
/// })).unwrap();
///
/// assert_eq!(2, v.s.len());
/// assert_eq!(1, v.s[&("World".to_string(), 456)]);
/// ```
pub mod btreemap_as_tuple_list {
    use super::*;

    /// Serialize the [`BTreeMap`] as a list of tuples
    pub fn serialize<K, V, S>(map: &BTreeMap<K, V>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
        K: Eq + Hash + Serialize,
        V: Serialize,
    {
        let mut seq = serializer.serialize_seq(Some(map.len()))?;
        for item in map.iter() {
            seq.serialize_element(&item)?;
        }
        seq.end()
    }

    /// Deserialize a [`BTreeMap`] from a list of tuples
    pub fn deserialize<'de, K, V, D>(deserializer: D) -> Result<BTreeMap<K, V>, D::Error>
    where
        D: Deserializer<'de>,
        K: Deserialize<'de> + Ord,
        V: Deserialize<'de>,
    {
        deserializer.deserialize_seq(BTreeMapVisitor(PhantomData))
    }

    #[cfg_attr(feature = "cargo-clippy", allow(type_complexity))]
    struct BTreeMapVisitor<K, V>(PhantomData<fn() -> BTreeMap<K, V>>);

    impl<'de, K, V> Visitor<'de> for BTreeMapVisitor<K, V>
    where
        K: Deserialize<'de> + Ord,
        V: Deserialize<'de>,
    {
        type Value = BTreeMap<K, V>;

        fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
            formatter.write_str("a list of key-value pairs")
        }

        fn visit_seq<A>(self, mut seq: A) -> Result<Self::Value, A::Error>
        where
            A: SeqAccess<'de>,
        {
            let mut map = BTreeMap::default();
            while let Some((key, value)) = seq.next_element()? {
                map.insert(key, value);
            }
            Ok(map)
        }
    }
}

/// This serializes a list of tuples into a map and back
///
/// Normally, you want to use a [`HashMap`] or a [`BTreeMap`] when deserializing a map.
/// However, sometimes this is not possible due to type contains, e.g., if the type implements neither [`Hash`] nor [`Ord`].
/// Another use case is deserializing a map with duplicate keys.
///
/// The implementation is generic using the [`FromIterator`] and [`IntoIterator`] traits.
/// Therefore, all of [`Vec`], [`VecDeque`](std::collections::VecDeque), and [`LinkedList`](std::collections::LinkedList) and anything which implements those are supported.
///
/// # Examples
///
/// `Wrapper` does not implement [`Hash`] nor [`Ord`], thus prohibiting the use [`HashMap`] or [`BTreeMap`].
///
/// ```
/// # extern crate serde;
/// # extern crate serde_derive;
/// # extern crate serde_json;
/// # extern crate serde_with;
/// #
/// # use serde_derive::{Deserialize, Serialize};
/// # use serde_json::json;
/// #
/// #[derive(Debug, Deserialize, Serialize, Default)]
/// struct S {
///     #[serde(with = "serde_with::rust::tuple_list_as_map")]
///     s: Vec<(Wrapper<i32>, Wrapper<String>)>,
/// }
///
/// #[derive(Clone, Debug, Serialize, Deserialize)]
/// #[serde(transparent)]
/// struct Wrapper<T>(T);
///
/// let from = r#"{
///   "s": {
///     "1": "Hi",
///     "2": "Cake",
///     "99": "Lie"
///   }
/// }"#;
/// let mut expected = S::default();
/// expected.s.push((Wrapper(1), Wrapper("Hi".into())));
/// expected.s.push((Wrapper(2), Wrapper("Cake".into())));
/// expected.s.push((Wrapper(99), Wrapper("Lie".into())));
///
/// let res: S = serde_json::from_str(from).unwrap();
/// for ((exp_k, exp_v), (res_k, res_v)) in expected.s.iter().zip(&res.s) {
///     assert_eq!(exp_k.0, res_k.0);
///     assert_eq!(exp_v.0, res_v.0);
/// }
/// assert_eq!(from, serde_json::to_string_pretty(&expected).unwrap());
/// ```
///
/// In this example, the serialized format contains duplicate keys, which is not supported with [`HashMap`] or [`BTreeMap`].
///
/// ```
/// # extern crate serde;
/// # extern crate serde_derive;
/// # extern crate serde_json;
/// # extern crate serde_with;
/// #
/// # use serde_derive::{Deserialize, Serialize};
/// # use serde_json::json;
/// #
/// #[derive(Debug, Deserialize, Serialize, PartialEq, Default)]
/// struct S {
///     #[serde(with = "serde_with::rust::tuple_list_as_map")]
///     s: Vec<(i32, String)>,
/// }
///
/// let from = r#"{
///   "s": {
///     "1": "Hi",
///     "1": "Cake",
///     "1": "Lie"
///   }
/// }"#;
/// let mut expected = S::default();
/// expected.s.push((1, "Hi".into()));
/// expected.s.push((1, "Cake".into()));
/// expected.s.push((1, "Lie".into()));
///
/// let res: S = serde_json::from_str(from).unwrap();
/// assert_eq!(3, res.s.len());
/// assert_eq!(expected, res);
/// assert_eq!(from, serde_json::to_string_pretty(&expected).unwrap());
/// ```
pub mod tuple_list_as_map {
    use super::{SerializeMap, *}; // Needed to remove the unused import warning in the parent scope

    /// Serialize any iteration of tuples into a map.
    pub fn serialize<'a, I, K, V, S>(iter: I, serializer: S) -> Result<S::Ok, S::Error>
    where
        I: IntoIterator<Item = &'a (K, V)>,
        I::IntoIter: ExactSizeIterator,
        K: Serialize + 'a,
        V: Serialize + 'a,
        S: Serializer,
    {
        let iter = iter.into_iter();
        let mut map = serializer.serialize_map(Some(iter.len()))?;
        for (key, value) in iter {
            map.serialize_entry(&key, &value)?;
        }
        map.end()
    }

    /// Deserialize a map into an iterator of tuples.
    pub fn deserialize<'de, I, K, V, D>(deserializer: D) -> Result<I, D::Error>
    where
        I: FromIterator<(K, V)>,
        K: Deserialize<'de>,
        V: Deserialize<'de>,
        D: Deserializer<'de>,
    {
        deserializer.deserialize_map(MapVisitor(PhantomData))
    }

    #[cfg_attr(feature = "cargo-clippy", allow(type_complexity))]
    struct MapVisitor<I, K, V>(PhantomData<fn() -> (I, K, V)>);

    impl<'de, I, K, V> Visitor<'de> for MapVisitor<I, K, V>
    where
        I: FromIterator<(K, V)>,
        K: Deserialize<'de>,
        V: Deserialize<'de>,
    {
        type Value = I;

        fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
            formatter.write_str("a map")
        }

        fn visit_map<A>(self, map: A) -> Result<Self::Value, A::Error>
        where
            A: MapAccess<'de>,
        {
            let iter = MapIter(map, PhantomData);
            iter.collect()
        }
    }

    struct MapIter<'de, A, K, V>(A, PhantomData<(&'de (), A, K, V)>);

    impl<'de, A, K, V> Iterator for MapIter<'de, A, K, V>
    where
        A: MapAccess<'de>,
        K: Deserialize<'de>,
        V: Deserialize<'de>,
    {
        type Item = Result<(K, V), A::Error>;

        fn next(&mut self) -> Option<Self::Item> {
            match self.0.next_entry() {
                Ok(Some(x)) => Some(Ok(x)),
                Ok(None) => None,
                Err(err) => Some(Err(err)),
            }
        }
    }
}

/// Deserialize from bytes or String
///
/// Any Rust [`String`] can be converted into bytes ([`Vec`]`<u8>`).
/// Accepting both as formats while deserializing can be helpful while interacting with language
/// which have a looser definition of string than Rust.
///
/// # Example
/// ```rust
/// # extern crate serde;
/// # extern crate serde_derive;
/// # extern crate serde_json;
/// # extern crate serde_with;
/// #
/// # use serde_derive::{Deserialize, Serialize};
/// # use serde_json::json;
/// #
/// #[derive(Debug, Deserialize, Serialize, PartialEq, Default)]
/// struct S {
///     #[serde(deserialize_with = "serde_with::rust::bytes_or_string::deserialize")]
///     bos: Vec<u8>,
/// }
///
/// // Here we deserialize from a byte array ...
/// let from = r#"{
///   "bos": [
///     0,
///     1,
///     2,
///     3
///   ]
/// }"#;
/// let expected = S {
///     bos: vec![0, 1, 2, 3],
/// };
///
/// let res: S = serde_json::from_str(from).unwrap();
/// assert_eq!(expected, res);
///
/// // and serialization works too.
/// assert_eq!(from, serde_json::to_string_pretty(&expected).unwrap());
///
/// // But we also support deserializing from String
/// let from = r#"{
///   "bos": "✨Works!"
/// }"#;
/// let expected = S {
///     bos: "✨Works!".as_bytes().to_vec(),
/// };
///
/// let res: S = serde_json::from_str(from).unwrap();
/// assert_eq!(expected, res);
/// ```
pub mod bytes_or_string {
    use super::*;

    /// Deserialize a [`Vec`]`<u8>` from either bytes or string
    pub fn deserialize<'de, D>(deserializer: D) -> Result<Vec<u8>, D::Error>
    where
        D: Deserializer<'de>,
    {
        deserializer.deserialize_any(BytesOrStringVisitor)
    }

    struct BytesOrStringVisitor;

    impl<'de> Visitor<'de> for BytesOrStringVisitor {
        type Value = Vec<u8>;

        fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
            formatter.write_str("a list of bytes or a string")
        }

        fn visit_bytes<E>(self, v: &[u8]) -> Result<Self::Value, E> {
            Ok(v.to_vec())
        }

        fn visit_byte_buf<E>(self, v: Vec<u8>) -> Result<Self::Value, E> {
            Ok(v)
        }

        fn visit_str<E>(self, v: &str) -> Result<Self::Value, E> {
            Ok(v.as_bytes().to_vec())
        }

        fn visit_string<E>(self, v: String) -> Result<Self::Value, E> {
            Ok(v.into_bytes())
        }

        fn visit_seq<A>(self, mut seq: A) -> Result<Self::Value, A::Error>
        where
            A: SeqAccess<'de>,
        {
            let mut res = Vec::with_capacity(seq.size_hint().unwrap_or(0));
            while let Some(value) = seq.next_element()? {
                res.push(value);
            }
            Ok(res)
        }
    }
}

/// Deserialize value and return [`Default`] on error
///
/// The main use case is ignoring error while deserializing.
/// Instead of erroring, it simply deserializes the [`Default`] variant of the type.
/// It is not possible to find the error location, i.e., which field had a deserialization error, with this method.
///
/// # Examples
///
/// ```
/// # extern crate serde;
/// # extern crate serde_derive;
/// # extern crate serde_json;
/// # extern crate serde_with;
/// #
/// # use serde_derive::Deserialize;
/// #
/// #[derive(Deserialize)]
/// struct A {
///     #[serde(deserialize_with = "serde_with::rust::default_on_error::deserialize")]
///     value: u32,
/// }
///
/// let a: A = serde_json::from_str(r#"{"value": 123}"#).unwrap();
/// assert_eq!(123, a.value);
///
/// // null is of invalid type
/// let a: A = serde_json::from_str(r#"{"value": null}"#).unwrap();
/// assert_eq!(0, a.value);
///
/// // String is of invalid type
/// let a: A = serde_json::from_str(r#"{"value": "123"}"#).unwrap();
/// assert_eq!(0, a.value);
///
/// // Missing entries still cause errors
/// assert!(serde_json::from_str::<A>(r#"{  }"#).is_err());
/// ```
///
/// Deserializing missing values can be supported by adding the `default` field attribute:
///
/// ```
/// # extern crate serde;
/// # extern crate serde_derive;
/// # extern crate serde_json;
/// # extern crate serde_with;
/// #
/// # use serde_derive::Deserialize;
/// #
/// #[derive(Deserialize)]
/// struct B {
///     #[serde(default, deserialize_with = "serde_with::rust::default_on_error::deserialize")]
///     value: u32,
/// }
///
///
/// let b: B = serde_json::from_str(r#"{  }"#).unwrap();
/// assert_eq!(0, b.value);
/// ```
pub mod default_on_error {
    use super::*;

    /// Deserialize T and return the [`Default`] value on error
    pub fn deserialize<'de, D, T>(deserializer: D) -> Result<T, D::Error>
    where
        D: Deserializer<'de>,
        T: Deserialize<'de> + Default,
    {
        T::deserialize(deserializer).or_else(|_| Ok(Default::default()))
    }
}

/// Deserialize default value if encountering `null`.
///
/// One use case are JSON APIs in which the `null` value represents some default state.
/// This adapter allows to turn the `null` directly into the [`Default`] value of the type.
///
/// # Examples
///
/// ```
/// # extern crate serde;
/// # extern crate serde_derive;
/// # extern crate serde_json;
/// # extern crate serde_with;
/// #
/// # use serde_derive::Deserialize;
/// #
/// #[derive(Deserialize)]
/// struct A {
///     #[serde(deserialize_with = "serde_with::rust::default_on_null::deserialize")]
///     value: u32,
/// }
///
/// let a: A = serde_json::from_str(r#"{"value": 123}"#).unwrap();
/// assert_eq!(123, a.value);
///
/// let a: A = serde_json::from_str(r#"{"value": null}"#).unwrap();
/// assert_eq!(0, a.value);
///
/// // String is invalid type
/// assert!(serde_json::from_str::<A>(r#"{"value": "123"}"#).is_err());
/// ```
pub mod default_on_null {
    use super::*;

    /// Deserialize T and return the [`Default`] value if original value is `null`
    pub fn deserialize<'de, D, T>(deserializer: D) -> Result<T, D::Error>
    where
        D: Deserializer<'de>,
        T: Deserialize<'de> + Default,
    {
        Ok(Option::deserialize(deserializer)?.unwrap_or_default())
    }
}
