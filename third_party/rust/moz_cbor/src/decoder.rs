use std::collections::BTreeMap;
use std::io::{Cursor, Read, Seek, SeekFrom};
use {CborError, CborType};

// We limit the length of any cbor byte array to 128MiB. This is a somewhat
// arbitrary limit that should work on all platforms and is large enough for
// any benign data.
pub const MAX_ARRAY_SIZE: usize = 134_217_728;

/// Struct holding a cursor and additional information for decoding.
#[derive(Debug)]
struct DecoderCursor<'a> {
    cursor: Cursor<&'a [u8]>,
}

/// Apply this mask (with &) to get the value part of the initial byte of a CBOR item.
const INITIAL_VALUE_MASK: u64 = 0b0001_1111;

impl<'a> DecoderCursor<'a> {
    /// Read and return the given number of bytes from the cursor. Advances the cursor.
    fn read_bytes(&mut self, len: usize) -> Result<Vec<u8>, CborError> {
        if len > MAX_ARRAY_SIZE {
            return Err(CborError::InputTooLarge);
        }
        let mut buf: Vec<u8> = vec![0; len];
        if self.cursor.read_exact(&mut buf).is_err() {
            Err(CborError::TruncatedInput)
        } else {
            Ok(buf)
        }
    }

    /// Convert num bytes to a u64
    fn read_uint_from_bytes(&mut self, num: usize) -> Result<u64, CborError> {
        let x = self.read_bytes(num)?;
        let mut result: u64 = 0;
        for i in (0..num).rev() {
            result += u64::from(x[num - 1 - i]) << (i * 8);
        }
        Ok(result)
    }

    /// Read an integer and return it as u64.
    fn read_int(&mut self) -> Result<u64, CborError> {
        let first_value = self.read_uint_from_bytes(1)? & INITIAL_VALUE_MASK;
        match first_value {
            0...23 => Ok(first_value),
            24 => self.read_uint_from_bytes(1),
            25 => self.read_uint_from_bytes(2),
            26 => self.read_uint_from_bytes(4),
            27 => self.read_uint_from_bytes(8),
            _ => Err(CborError::MalformedInput),
        }
    }

    fn read_negative_int(&mut self) -> Result<CborType, CborError> {
        let uint = self.read_int()?;
        if uint > i64::max_value() as u64 {
            return Err(CborError::InputValueOutOfRange);
        }
        let result: i64 = -1 - uint as i64;
        Ok(CborType::SignedInteger(result))
    }

    /// Read an array of data items and return it.
    fn read_array(&mut self) -> Result<CborType, CborError> {
        // Create a new array.
        let mut array: Vec<CborType> = Vec::new();
        // Read the length of the array.
        let num_items = self.read_int()?;
        // Decode each of the num_items data items.
        for _ in 0..num_items {
            let new_item = self.decode_item()?;
            array.push(new_item);
        }
        Ok(CborType::Array(array))
    }

    /// Read a byte string and return it.
    fn read_byte_string(&mut self) -> Result<CborType, CborError> {
        let length = self.read_int()?;
        if length > MAX_ARRAY_SIZE as u64 {
            return Err(CborError::InputTooLarge);
        }
        let byte_string = self.read_bytes(length as usize)?;
        Ok(CborType::Bytes(byte_string))
    }

    /// Read a map.
    fn read_map(&mut self) -> Result<CborType, CborError> {
        let num_items = self.read_int()?;
        // Create a new array.
        let mut map: BTreeMap<CborType, CborType> = BTreeMap::new();
        // Decode each of the num_items (key, data item) pairs.
        for _ in 0..num_items {
            let key_val = self.decode_item()?;
            let item_value = self.decode_item()?;
            if map.insert(key_val, item_value).is_some() {
                return Err(CborError::DuplicateMapKey);
            }
        }
        Ok(CborType::Map(map))
    }

    fn read_null(&mut self) -> Result<CborType, CborError> {
        let value = self.read_uint_from_bytes(1)? & INITIAL_VALUE_MASK;
        if value != 22 {
            return Err(CborError::UnsupportedType);
        }
        Ok(CborType::Null)
    }

    /// Peeks at the next byte in the cursor, but does not change the position.
    fn peek_byte(&mut self) -> Result<u8, CborError> {
        let x = self.read_bytes(1)?;
        if self.cursor.seek(SeekFrom::Current(-1)).is_err() {
            return Err(CborError::LibraryError);
        };
        Ok(x[0])
    }

    /// Decodes the next CBOR item.
    pub fn decode_item(&mut self) -> Result<CborType, CborError> {
        let major_type = self.peek_byte()? >> 5;
        match major_type {
            0 => {
                let value = self.read_int()?;
                Ok(CborType::Integer(value))
            }
            1 => self.read_negative_int(),
            2 => self.read_byte_string(),
            4 => self.read_array(),
            5 => self.read_map(),
            6 => {
                let tag = self.read_int()?;
                let item = self.decode_item()?;
                Ok(CborType::Tag(tag, Box::new(item)))
            }
            7 => self.read_null(),
            _ => Err(CborError::UnsupportedType),
        }
    }
}

/// Read the CBOR structure in bytes and return it as a `CborType`.
pub fn decode(bytes: &[u8]) -> Result<CborType, CborError> {
    let mut decoder_cursor = DecoderCursor { cursor: Cursor::new(bytes) };
    decoder_cursor.decode_item()
    // TODO: check cursor at end?
}
