use serde_bytes::{ByteBuf, Bytes};
use serde_derive::{Deserialize, Serialize};
use std::borrow::Cow;

#[derive(Serialize, Deserialize, PartialEq, Debug)]
struct Test<'a> {
    #[serde(with = "serde_bytes")]
    slice: &'a [u8],

    #[serde(with = "serde_bytes")]
    vec: Vec<u8>,

    #[serde(with = "serde_bytes")]
    bytes: &'a Bytes,

    #[serde(with = "serde_bytes")]
    byte_buf: ByteBuf,

    #[serde(with = "serde_bytes")]
    cow_slice: Cow<'a, [u8]>,

    #[serde(with = "serde_bytes")]
    cow_bytes: Cow<'a, Bytes>,

    #[serde(with = "serde_bytes")]
    boxed_slice: Box<[u8]>,

    #[serde(with = "serde_bytes")]
    boxed_bytes: Box<Bytes>,
}

#[derive(Serialize)]
struct Dst {
    #[serde(with = "serde_bytes")]
    bytes: [u8],
}
