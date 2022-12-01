use serde::de;
use serde_cbor::error::Result;
use serde_cbor::Deserializer;

pub fn from_slice_stream<'a, T>(slice: &'a [u8]) -> Result<(&'a [u8], T)>
where
    T: de::Deserialize<'a>,
{
    let mut deserializer = Deserializer::from_slice(slice);
    let value = de::Deserialize::deserialize(&mut deserializer)?;
    let rest = &slice[deserializer.byte_offset()..];

    Ok((rest, value))
}
