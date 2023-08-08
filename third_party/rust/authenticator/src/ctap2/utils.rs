use serde::de;
use serde_cbor::Deserializer;
use std::io::Read;

pub fn serde_parse_err<E: de::Error>(s: &str) -> E {
    E::custom(format!("Failed to parse {s}"))
}

pub fn from_slice_stream<'a, T, R: Read, E: de::Error>(data: &mut R) -> Result<T, E>
where
    T: de::Deserialize<'a>,
{
    let mut deserializer = Deserializer::from_reader(data);
    de::Deserialize::deserialize(&mut deserializer)
        .map_err(|x| serde_parse_err(&format!("{}: {}", stringify!(T), &x.to_string())))
}

// Parsing routines

pub fn read_be_u32<R: Read, E: de::Error>(data: &mut R) -> Result<u32, E> {
    let mut buf = [0; 4];
    data.read_exact(&mut buf)
        .map_err(|_| serde_parse_err("u32"))?;
    Ok(u32::from_be_bytes(buf))
}

pub fn read_be_u16<R: Read, E: de::Error>(data: &mut R) -> Result<u16, E> {
    let mut buf = [0; 2];
    data.read_exact(&mut buf)
        .map_err(|_| serde_parse_err("u16"))?;
    Ok(u16::from_be_bytes(buf))
}

pub fn read_byte<R: Read, E: de::Error>(data: &mut R) -> Result<u8, E> {
    match data.bytes().next() {
        Some(Ok(s)) => Ok(s),
        _ => Err(serde_parse_err("u8")),
    }
}
