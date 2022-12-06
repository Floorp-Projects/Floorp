#[macro_use]
extern crate serde_derive;

use serde::Serialize;
use serde_cbor::ser::SliceWrite;
use serde_cbor::{self, Serializer};

#[derive(Debug, PartialEq, Serialize, Deserialize)]
struct Example {
    foo: Foo,
    payload: u8,
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
struct Foo {
    x: u8,
    color: Color,
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
enum Color {
    Red,
    Blue,
    Yellow(u8),
}

const EXAMPLE: Example = Example {
    foo: Foo {
        x: 0xAA,
        color: Color::Yellow(40),
    },
    payload: 0xCC,
};

#[cfg(feature = "std")]
mod std_tests {
    use super::*;

    #[test]
    fn test() {
        let serialized = serde_cbor::ser::to_vec_packed(&EXAMPLE).expect("bennofs 1");
        println!("{:?}", serialized);
        let deserialized: Example = serde_cbor::from_slice(&serialized).expect("bennofs 2");
        assert_eq!(EXAMPLE, deserialized);
    }
}

#[test]
fn test() {
    let mut slice = [0u8; 64];
    let writer = SliceWrite::new(&mut slice);
    let mut serializer = Serializer::new(writer).packed_format();
    EXAMPLE.serialize(&mut serializer).unwrap();
    let writer = serializer.into_inner();
    let end = writer.bytes_written();
    let slice = writer.into_inner();
    let deserialized: Example =
        serde_cbor::de::from_slice_with_scratch(&slice[..end], &mut []).unwrap();
    assert_eq!(EXAMPLE, deserialized);
}
