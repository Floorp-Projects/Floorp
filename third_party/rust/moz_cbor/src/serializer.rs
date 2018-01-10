use std::collections::BTreeMap;
use CborType;

/// Given a vector of bytes to append to, a tag to use, and an unsigned value to encode, uses the
/// CBOR unsigned integer encoding to represent the given value.
fn common_encode_unsigned(output: &mut Vec<u8>, tag: u8, value: u64) {
    assert!(tag < 8);
    let shifted_tag = tag << 5;
    match value {
        0...23 => {
            output.push(shifted_tag | (value as u8));
        }
        24...255 => {
            output.push(shifted_tag | 24);
            output.push(value as u8);
        }
        256...65_535 => {
            output.push(shifted_tag | 25);
            output.push((value >> 8) as u8);
            output.push((value & 255) as u8);
        }
        65_536...4_294_967_295 => {
            output.push(shifted_tag | 26);
            output.push((value >> 24) as u8);
            output.push(((value >> 16) & 255) as u8);
            output.push(((value >> 8) & 255) as u8);
            output.push((value & 255) as u8);
        }
        _ => {
            output.push(shifted_tag | 27);
            output.push((value >> 56) as u8);
            output.push(((value >> 48) & 255) as u8);
            output.push(((value >> 40) & 255) as u8);
            output.push(((value >> 32) & 255) as u8);
            output.push(((value >> 24) & 255) as u8);
            output.push(((value >> 16) & 255) as u8);
            output.push(((value >> 8) & 255) as u8);
            output.push((value & 255) as u8);
        }
    };
}

/// The major type is 0. For values 0 through 23, the 5 bits of additional information is just the
/// value of the unsigned number. For values representable in one byte, the additional information
/// has the value 24. If two bytes are necessary, the value is 25. If four bytes are necessary, the
/// value is 26. If 8 bytes are necessary, the value is 27. The following bytes are the value of the
/// unsigned number in as many bytes were indicated in network byte order (big endian).
fn encode_unsigned(output: &mut Vec<u8>, unsigned: u64) {
    common_encode_unsigned(output, 0, unsigned);
}

/// The major type is 1. The encoding is the same as for positive (i.e. unsigned) integers, except
/// the value encoded is -1 minus the value of the negative number.
fn encode_negative(output: &mut Vec<u8>, negative: i64) {
    assert!(negative < 0);
    let value_to_encode: u64 = (-1 - negative) as u64;
    common_encode_unsigned(output, 1, value_to_encode);
}

/// The major type is 2. The length of the data is encoded as with positive integers, followed by
/// the actual data.
fn encode_bytes(output: &mut Vec<u8>, bstr: &[u8]) {
    common_encode_unsigned(output, 2, bstr.len() as u64);
    output.extend_from_slice(bstr);
}

/// The major type is 3. The length is as with bstr. The UTF-8-encoded bytes of the string follow.
fn encode_string(output: &mut Vec<u8>, tstr: &str) {
    let utf8_bytes = tstr.as_bytes();
    common_encode_unsigned(output, 3, utf8_bytes.len() as u64);
    output.extend_from_slice(utf8_bytes);
}

/// The major type is 4. The number of items is encoded as with positive integers. Then follows the
/// encodings of the items themselves.
fn encode_array(output: &mut Vec<u8>, array: &[CborType]) {
    common_encode_unsigned(output, 4, array.len() as u64);
    for element in array {
        output.append(&mut element.serialize());
    }
}

/// The major type is 5. The number of pairs is encoded as with positive integers. Then follows the
/// encodings of each key, value pair. In Canonical CBOR, the keys must be sorted lowest value to
/// highest.
fn encode_map(output: &mut Vec<u8>, map: &BTreeMap<CborType, CborType>) {
    common_encode_unsigned(output, 5, map.len() as u64);
    for (key, value) in map {
        output.append(&mut key.serialize());
        output.append(&mut value.serialize());
    }
}

fn encode_tag(output: &mut Vec<u8>, tag: &u64, val: &CborType) {
    common_encode_unsigned(output, 6, *tag);
    output.append(&mut val.serialize());
}

/// The major type is 7. The only supported value for this type is 22, which is Null.
/// This makes the encoded value 246, or 0xf6.
fn encode_null(output: &mut Vec<u8>) {
    output.push(0xf6);
}

impl CborType {
    /// Serialize a Cbor object.
    pub fn serialize(&self) -> Vec<u8> {
        let mut bytes: Vec<u8> = Vec::new();
        match *self {
            CborType::Integer(ref unsigned) => encode_unsigned(&mut bytes, *unsigned),
            CborType::SignedInteger(ref negative) => encode_negative(&mut bytes, *negative),
            CborType::Bytes(ref bstr) => encode_bytes(&mut bytes, bstr),
            CborType::String(ref tstr) => encode_string(&mut bytes, tstr),
            CborType::Array(ref arr) => encode_array(&mut bytes, arr),
            CborType::Map(ref map) => encode_map(&mut bytes, map),
            CborType::Tag(ref t, ref val) => encode_tag(&mut bytes, t, val),
            CborType::Null => encode_null(&mut bytes),
        };
        bytes
    }
}
