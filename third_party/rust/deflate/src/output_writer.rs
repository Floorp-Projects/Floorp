use std::u16;

use lzvalue::LZValue;
use huffman_table::{NUM_LITERALS_AND_LENGTHS, NUM_DISTANCE_CODES, END_OF_BLOCK_POSITION,
                    get_distance_code, get_length_code};

/// The type used for representing how many times a literal, length or distance code has been ouput
/// to the current buffer.
/// As we are limiting the blocks to be at most 2^16 bytes long, we can represent frequencies using
/// 16-bit values.
pub type FrequencyType = u16;

/// The maximum number of literals/lengths in the buffer, which in practice also means the maximum
/// number of literals/lengths output before a new block is started.
/// This should not be larger than the maximum value `FrequencyType` can represent to prevent
/// overflowing (which would degrade, or in the worst case break compression).
pub const MAX_BUFFER_LENGTH: usize = 1024 * 31;

#[derive(Debug, PartialEq)]
pub enum BufferStatus {
    NotFull,
    Full,
}

/// Struct that buffers lz77 data and keeps track of the usage of different codes
pub struct DynamicWriter {
    buffer: Vec<LZValue>,
    // The two last length codes are not actually used, but only participates in code construction
    // Therefore, we ignore them to get the correct number of lengths
    frequencies: [FrequencyType; NUM_LITERALS_AND_LENGTHS],
    distance_frequencies: [FrequencyType; NUM_DISTANCE_CODES],
}

impl DynamicWriter {
    #[inline]
    pub fn check_buffer_length(&self) -> BufferStatus {
        if self.buffer.len() >= MAX_BUFFER_LENGTH {
            BufferStatus::Full
        } else {
            BufferStatus::NotFull
        }
    }

    #[inline]
    pub fn write_literal(&mut self, literal: u8) -> BufferStatus {
        debug_assert!(self.buffer.len() < MAX_BUFFER_LENGTH);
        self.buffer.push(LZValue::literal(literal));
        self.frequencies[usize::from(literal)] += 1;
        self.check_buffer_length()
    }

    #[inline]
    pub fn write_length_distance(&mut self, length: u16, distance: u16) -> BufferStatus {
        self.buffer.push(LZValue::length_distance(length, distance));
        let l_code_num = get_length_code(length);
        // As we limit the buffer to 2^16 values, this should be safe from overflowing.
        if cfg!(debug_assertions) {
            self.frequencies[l_code_num] += 1;
        } else {
            // #Safety
            // None of the values in the table of length code numbers will give a value
            // that is out of bounds.
            // There is a test to ensure that these functions can not produce too large values.
            unsafe {
                *self.frequencies.get_unchecked_mut(l_code_num) += 1;
            }
        }
        let d_code_num = get_distance_code(distance);
        // The compiler seems to be able to evade the bounds check here somehow.
        self.distance_frequencies[usize::from(d_code_num)] += 1;
        self.check_buffer_length()
    }

    pub fn buffer_length(&self) -> usize {
        self.buffer.len()
    }

    pub fn get_buffer(&self) -> &[LZValue] {
        &self.buffer
    }

    pub fn new() -> DynamicWriter {
        let mut w = DynamicWriter {
            buffer: Vec::with_capacity(MAX_BUFFER_LENGTH),
            frequencies: [0; NUM_LITERALS_AND_LENGTHS],
            distance_frequencies: [0; NUM_DISTANCE_CODES],
        };
        // This will always be 1,
        // since there will always only be one end of block marker in each block
        w.frequencies[END_OF_BLOCK_POSITION] = 1;
        w
    }

    /// Special output function used with RLE compression
    /// that avoids bothering to lookup a distance code.
    #[inline]
    pub fn write_length_rle(&mut self, length: u16) -> BufferStatus {
        self.buffer.push(LZValue::length_distance(length, 1));
        let l_code_num = get_length_code(length);
        // As we limit the buffer to 2^16 values, this should be safe from overflowing.
        if cfg!(debug_assertions) {
            self.frequencies[l_code_num] += 1;
        } else {
            // #Safety
            // None of the values in the table of length code numbers will give a value
            // that is out of bounds.
            // There is a test to ensure that these functions won't produce too large values.
            unsafe {
                *self.frequencies.get_unchecked_mut(l_code_num) += 1;
            }
        }
        self.distance_frequencies[0] += 1;
        self.check_buffer_length()
    }

    pub fn get_frequencies(&self) -> (&[u16], &[u16]) {
        (&self.frequencies, &self.distance_frequencies)
    }

    pub fn clear_frequencies(&mut self) {
        self.frequencies = [0; NUM_LITERALS_AND_LENGTHS];
        self.distance_frequencies = [0; NUM_DISTANCE_CODES];
        self.frequencies[END_OF_BLOCK_POSITION] = 1;
    }

    pub fn clear_data(&mut self) {
        self.buffer.clear()
    }

    pub fn clear(&mut self) {
        self.clear_frequencies();
        self.clear_data();
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use huffman_table::{get_length_code, get_distance_code};
    #[test]
    /// Ensure that these function won't produce values that would overflow the output_writer
    /// tables since we use some unsafe indexing.
    fn array_bounds() {
        let w = DynamicWriter::new();

        for i in 0..u16::max_value() {
            get_length_code(i) < w.frequencies.len();
        }

        for i in 0..u16::max_value() {
            get_distance_code(i) < w.distance_frequencies.len() as u8;
        }
    }
}
