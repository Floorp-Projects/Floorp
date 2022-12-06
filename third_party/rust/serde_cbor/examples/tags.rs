use serde::de::{Deserialize, Deserializer};
use serde::ser::{Serialize, Serializer};
use serde_cbor::tags::Tagged;
use serde_cbor::Value;
use serde_derive::{Deserialize, Serialize};
use std::error::Error;

/// https://tools.ietf.org/html/rfc7049#section-2.4.1
#[derive(Debug, PartialEq)]
struct Date(String);

impl Serialize for Date {
    fn serialize<S: Serializer>(&self, s: S) -> Result<S::Ok, S::Error> {
        Tagged::new(Some(0), &self.0).serialize(s)
    }
}

impl<'de> Deserialize<'de> for Date {
    fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        let tagged = Tagged::<String>::deserialize(deserializer)?;
        match tagged.tag {
            Some(0) | None => Ok(Date(tagged.value)),
            Some(_) => Err(serde::de::Error::custom("unexpected tag")),
        }
    }
}

/// https://tools.ietf.org/html/rfc7049#section-2.4.4.3
#[derive(Debug, PartialEq)]
struct Uri(String);

impl Serialize for Uri {
    fn serialize<S: Serializer>(&self, s: S) -> Result<S::Ok, S::Error> {
        Tagged::new(Some(32), &self.0).serialize(s)
    }
}
impl<'de> Deserialize<'de> for Uri {
    fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        let tagged = Tagged::<String>::deserialize(deserializer)?;
        match tagged.tag {
            // allow deserialization even if there is no tag. Allows roundtrip via other formats such as json
            Some(0) | None => Ok(Uri(tagged.value)),
            Some(_) => Err(serde::de::Error::custom("unexpected tag")),
        }
    }
}

#[derive(Debug, Serialize, Deserialize, PartialEq)]
struct Bookmark {
    title: String,
    link: Uri,
    created: Date,
}

fn main() -> Result<(), Box<dyn Error>> {
    let bookmark = Bookmark {
        title: "The Example Domain".into(),
        link: Uri("http://example.org/".into()),
        created: Date("2003-12-13T18:30:02Z".into()),
    };

    // serialize the struct to bytes
    let bytes1 = serde_cbor::to_vec(&bookmark)?;
    // deserialize to a serde_cbor::Value
    let value1: Value = serde_cbor::from_slice(&bytes1)?;
    println!("{:?}", value1);
    // serialize the value to bytes
    let bytes2 = serde_cbor::to_vec(&value1)?;
    // deserialize to a serde_cbor::Value
    let value2: Value = serde_cbor::from_slice(&bytes2)?;
    println!("{:?}", value2);
    // deserialize to a Bookmark
    let result: Bookmark = serde_cbor::from_slice(&bytes2)?;

    // check that the roundtrip was successful
    assert_eq!(value1, value2);
    assert_eq!(bookmark, result);

    // check that going via a format that does not support tags does work
    // let json = serde_json::to_vec(&bookmark)?;
    // let result: Bookmark = serde_json::from_slice(&json)?;
    // assert_eq!(bookmark, result);
    Ok(())
}
