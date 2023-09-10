#![doc = include_str!("../README.md")]
#![doc(html_root_url = "https://docs.rs/ron/0.8.1")]

pub mod de;
pub mod ser;

pub mod error;
pub mod value;

pub mod extensions;

pub mod options;

pub use de::{from_str, Deserializer};
pub use error::{Error, Result};
pub use options::Options;
pub use ser::{to_string, Serializer};
pub use value::{Map, Number, Value};

mod parse;
