use std::io::Write;
use std::io;
use std::u16;
use bitstream::LsbWriter;
use byteorder::{LittleEndian, WriteBytesExt};

#[cfg(test)]
const BLOCK_SIZE: u16 = 32000;

const STORED_FIRST_BYTE: u8 = 0b0000_0000;
pub const STORED_FIRST_BYTE_FINAL: u8 = 0b0000_0001;
pub const MAX_STORED_BLOCK_LENGTH: usize = (u16::MAX as usize) / 2;

pub fn write_stored_header(writer: &mut LsbWriter, final_block: bool) {
    let header = if final_block {
        STORED_FIRST_BYTE_FINAL
    } else {
        STORED_FIRST_BYTE
    };
    // Write the block header
    writer.write_bits(header.into(), 3);
    // Flush the writer to make sure we are aligned to the byte boundary.
    writer.flush_raw();
}

// Compress one stored block (excluding the header)
pub fn compress_block_stored<W: Write>(input: &[u8], writer: &mut W) -> io::Result<usize> {
    if input.len() > u16::max_value() as usize {
        return Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            "Stored block too long!",
        ));
    };
    // The header is written before this function.
    // The next two bytes indicates the length
    writer.write_u16::<LittleEndian>(input.len() as u16)?;
    // the next two after the length is the ones complement of the length
    writer.write_u16::<LittleEndian>(!input.len() as u16)?;
    // After this the data is written directly with no compression
    writer.write(input)
}

#[cfg(test)]
pub fn compress_data_stored(input: &[u8]) -> Vec<u8> {
    let block_length = BLOCK_SIZE as usize;

    let mut output = Vec::with_capacity(input.len() + 2);
    let mut i = input.chunks(block_length).peekable();
    while let Some(chunk) = i.next() {
        let last_chunk = i.peek().is_none();
        // First bit tells us if this is the final chunk
        // the next two details compression type (none in this case)
        let first_byte = if last_chunk {
            STORED_FIRST_BYTE_FINAL
        } else {
            STORED_FIRST_BYTE
        };
        output.write(&[first_byte]).unwrap();

        compress_block_stored(chunk, &mut output).unwrap();
    }
    output
}


#[cfg(test)]
mod test {
    use super::*;
    use test_utils::decompress_to_end;

    #[test]
    fn no_compression_one_chunk() {
        let test_data = vec![1u8, 2, 3, 4, 5, 6, 7, 8];
        let compressed = compress_data_stored(&test_data);
        let result = decompress_to_end(&compressed);
        assert_eq!(test_data, result);
    }

    #[test]
    fn no_compression_multiple_chunks() {
        let test_data = vec![32u8; 40000];
        let compressed = compress_data_stored(&test_data);
        let result = decompress_to_end(&compressed);
        assert_eq!(test_data, result);
    }

    #[test]
    fn no_compression_string() {
        let test_data = String::from(
            "This is some text, this is some more text, this is even \
             more text, lots of text here.",
        ).into_bytes();
        let compressed = compress_data_stored(&test_data);
        let result = decompress_to_end(&compressed);
        assert_eq!(test_data, result);
    }
}
