#[cfg(test)]
use std::mem;
use huffman_table::HuffmanTable;
use bitstream::LsbWriter;
use lzvalue::LZType;

// The first bits of each block, which describe the type of the block
// `-TTF` - TT = type, 00 = stored, 01 = fixed, 10 = dynamic, 11 = reserved, F - 1 if final block
// `0000`;
const FIXED_FIRST_BYTE: u16 = 0b010;
const FIXED_FIRST_BYTE_FINAL: u16 = 0b011;
const DYNAMIC_FIRST_BYTE: u16 = 0b100;
const DYNAMIC_FIRST_BYTE_FINAL: u16 = 0b101;

#[allow(dead_code)]
pub enum BType {
    NoCompression = 0b00,
    FixedHuffman = 0b01,
    DynamicHuffman = 0b10, // Reserved = 0b11, //Error
}

/// A struct wrapping a writer that writes data compressed using the provided huffman table
pub struct EncoderState {
    pub huffman_table: HuffmanTable,
    pub writer: LsbWriter,
}

impl EncoderState {
    /// Creates a new encoder state using the provided huffman table and writer
    pub fn new(writer: Vec<u8>) -> EncoderState {
        EncoderState {
            huffman_table: HuffmanTable::empty(),
            writer: LsbWriter::new(writer),
        }
    }

    #[cfg(test)]
    /// Creates a new encoder state using the fixed huffman table
    pub fn fixed(writer: Vec<u8>) -> EncoderState {
        EncoderState {
            huffman_table: HuffmanTable::fixed_table(),
            writer: LsbWriter::new(writer),
        }
    }

    pub fn inner_vec(&mut self) -> &mut Vec<u8> {
        &mut self.writer.w
    }

    /// Encodes a literal value to the writer
    fn write_literal(&mut self, value: u8) {
        let code = self.huffman_table.get_literal(value);
        debug_assert!(code.length > 0);
        self.writer.write_bits(code.code, code.length);
    }

    /// Write a LZvalue to the contained writer, returning Err if the write operation fails
    pub fn write_lzvalue(&mut self, value: LZType) {
        match value {
            LZType::Literal(l) => self.write_literal(l),
            LZType::StoredLengthDistance(l, d) => {
                let (code, extra_bits_code) = self.huffman_table.get_length_huffman(l);
                debug_assert!(
                    code.length != 0,
                    format!("Code: {:?}, Value: {:?}", code, value)
                );
                self.writer.write_bits(code.code, code.length);
                self.writer
                    .write_bits(extra_bits_code.code, extra_bits_code.length);

                let (code, extra_bits_code) = self.huffman_table.get_distance_huffman(d);
                debug_assert!(
                    code.length != 0,
                    format!("Code: {:?}, Value: {:?}", code, value)
                );

                self.writer.write_bits(code.code, code.length);
                self.writer
                    .write_bits(extra_bits_code.code, extra_bits_code.length)
            }
        };
    }

    /// Write the start of a block, returning Err if the write operation fails.
    pub fn write_start_of_block(&mut self, fixed: bool, final_block: bool) {
        if final_block {
            // The final block has one bit flipped to indicate it's
            // the final one
            if fixed {
                self.writer.write_bits(FIXED_FIRST_BYTE_FINAL, 3)
            } else {
                self.writer.write_bits(DYNAMIC_FIRST_BYTE_FINAL, 3)
            }
        } else if fixed {
            self.writer.write_bits(FIXED_FIRST_BYTE, 3)
        } else {
            self.writer.write_bits(DYNAMIC_FIRST_BYTE, 3)
        }
    }

    /// Write the end of block code
    pub fn write_end_of_block(&mut self) {
        let code = self.huffman_table.get_end_of_block();
        self.writer.write_bits(code.code, code.length)
    }

    /// Flush the contained writer and it's bitstream wrapper.
    pub fn flush(&mut self) {
        self.writer.flush_raw()
    }

    pub fn set_huffman_to_fixed(&mut self) {
        self.huffman_table.set_to_fixed()
    }

    /// Reset the encoder state with a new writer, returning the old one if flushing
    /// succeeds.
    #[cfg(test)]
    pub fn reset(&mut self, writer: Vec<u8>) -> Vec<u8> {
        // Make sure the writer is flushed
        // Ideally this should be done before this function is called, but we
        // do it here just in case.
        self.flush();
        // Reset the huffman table
        // This probably isn't needed, but again, we do it just in case to avoid leaking any data
        // If this turns out to be a performance issue, it can probably be ignored later.
        self.huffman_table = HuffmanTable::empty();
        mem::replace(&mut self.writer.w, writer)
    }
}
