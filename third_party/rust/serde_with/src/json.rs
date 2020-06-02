//! De/Serialization of JSON
//!
//! This modules is only available if using the `json` feature of the crate.

/// Serialize value as string containing JSON
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
///     #[serde(with = "serde_with::json::nested")]
///     other_struct: B,
/// }
/// #[derive(Deserialize, Serialize)]
/// struct B {
///     value: usize,
/// }
///
/// let v: A = serde_json::from_str(r#"{"other_struct":"{\"value\":5}"}"#).unwrap();
/// assert_eq!(5, v.other_struct.value);
///
/// let x = A {
///     other_struct: B { value: 10 },
/// };
/// assert_eq!(r#"{"other_struct":"{\"value\":10}"}"#, serde_json::to_string(&x).unwrap());
/// ```
///
pub mod nested {
    use serde::{
        de::{DeserializeOwned, Deserializer, Error, Visitor},
        ser::{self, Serialize, Serializer},
    };
    use serde_json;
    use std::{fmt, marker::PhantomData};

    /// Deserialize value from a string which is valid JSON
    pub fn deserialize<'de, D, T>(deserializer: D) -> Result<T, D::Error>
    where
        D: Deserializer<'de>,
        T: DeserializeOwned,
    {
        #[derive(Default)]
        struct Helper<S: DeserializeOwned>(PhantomData<S>);

        impl<'de, S> Visitor<'de> for Helper<S>
        where
            S: DeserializeOwned,
        {
            type Value = S;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                write!(formatter, "valid json object")
            }

            fn visit_str<E>(self, value: &str) -> Result<S, E>
            where
                E: Error,
            {
                serde_json::from_str(value).map_err(Error::custom)
            }
        }

        deserializer.deserialize_str(Helper(PhantomData))
    }

    /// Serialize value as string containing JSON
    ///
    /// # Errors
    ///
    /// Serialization can fail if `T`'s implementation of `Serialize` decides to
    /// fail, or if `T` contains a map with non-string keys.
    pub fn serialize<T, S>(value: &T, serializer: S) -> Result<S::Ok, S::Error>
    where
        T: Serialize,
        S: Serializer,
    {
        let s = serde_json::to_string(value).map_err(ser::Error::custom)?;
        serializer.serialize_str(&*s)
    }
}
