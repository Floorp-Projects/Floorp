use ::HeaderValue;

//pub use self::charset::Charset;
//pub use self::encoding::Encoding;
pub(crate) use self::entity::EntityTag;
pub(crate) use self::flat_csv::{FlatCsv, SemiColon};
pub(crate) use self::fmt::fmt;
pub(crate) use self::http_date::HttpDate;
pub(crate) use self::iter::IterExt;
//pub use language_tags::LanguageTag;
//pub use self::quality_value::{Quality, QualityValue};
pub(crate) use self::seconds::Seconds;
pub(crate) use self::value_string::HeaderValueString;

//mod charset;
pub(crate) mod csv;
//mod encoding;
mod entity;
mod flat_csv;
mod fmt;
mod http_date;
mod iter;
//mod quality_value;
mod seconds;
mod value_string;

macro_rules! error_type {
    ($name:ident) => (
        #[doc(hidden)]
        pub struct $name {
            _inner: (),
        }

        impl ::std::fmt::Debug for $name {
            fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
                f.debug_struct(stringify!($name))
                    .finish()
            }
        }

        impl ::std::fmt::Display for $name {
            fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
                f.write_str(stringify!($name))
            }
        }

        impl ::std::error::Error for $name {
            fn description(&self) -> &str {
                stringify!($name)
            }
        }
    );
}

/// A helper trait for use when deriving `Header`.
pub(crate) trait TryFromValues: Sized {
    /// Try to convert from the values into an instance of `Self`.
    fn try_from_values<'i, I>(values: &mut I) -> Result<Self, ::Error>
    where
        Self: Sized,
        I: Iterator<Item = &'i HeaderValue>;
}

impl TryFromValues for HeaderValue {
    fn try_from_values<'i, I>(values: &mut I) -> Result<Self, ::Error>
    where
        I: Iterator<Item = &'i HeaderValue>,
    {
        values
            .next()
            .cloned()
            .ok_or_else(|| ::Error::invalid())
    }
}

