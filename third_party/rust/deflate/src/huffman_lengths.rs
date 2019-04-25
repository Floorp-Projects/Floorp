use length_encode::{EncodedLength, encode_lengths_m, huffman_lengths_from_frequency_m,
                    COPY_PREVIOUS, REPEAT_ZERO_3_BITS, REPEAT_ZERO_7_BITS};
use huffman_table::{HuffmanTable, create_codes_in_place, num_extra_bits_for_length_code,
                    num_extra_bits_for_distance_code, NUM_LITERALS_AND_LENGTHS,
                    NUM_DISTANCE_CODES, MAX_CODE_LENGTH, FIXED_CODE_LENGTHS, LENGTH_BITS_START};
use bitstream::LsbWriter;
use output_writer::FrequencyType;
use stored_block::MAX_STORED_BLOCK_LENGTH;
use deflate_state::LengthBuffers;

use std::cmp;

// The minimum number of literal/length values
pub const MIN_NUM_LITERALS_AND_LENGTHS: usize = 257;
// The minimum number of distances
pub const MIN_NUM_DISTANCES: usize = 1;

const NUM_HUFFMAN_LENGTHS: usize = 19;

// The output ordering of the lengths for the huffman codes used to encode the lengths
// used to build the full huffman tree for length/literal codes.
// http://www.gzip.org/zlib/rfc-deflate.html#dyn
const HUFFMAN_LENGTH_ORDER: [u8; NUM_HUFFMAN_LENGTHS] = [
    16,
    17,
    18,
    0,
    8,
    7,
    9,
    6,
    10,
    5,
    11,
    4,
    12,
    3,
    13,
    2,
    14,
    1,
    15,
];

// Number of bits used for the values specifying the number of codes
const HLIT_BITS: u8 = 5;
const HDIST_BITS: u8 = 5;
const HCLEN_BITS: u8 = 4;

// The longest a huffman code describing another huffman length can be
const MAX_HUFFMAN_CODE_LENGTH: usize = 7;

// How many bytes (not including padding and the 3-bit block type) the stored block header takes up.
const STORED_BLOCK_HEADER_LENGTH: u64 = 4;
const BLOCK_MARKER_LENGTH: u8 = 3;

/// Creates a new slice from the input slice that stops at the final non-zero value
pub fn remove_trailing_zeroes<T: From<u8> + PartialEq>(input: &[T], min_length: usize) -> &[T] {
    let num_zeroes = input.iter().rev().take_while(|&a| *a == T::from(0)).count();
    &input[0..cmp::max(input.len() - num_zeroes, min_length)]
}

/// How many extra bits the huffman length code uses to represent a value.
fn extra_bits_for_huffman_length_code(code: u8) -> u8 {
    match code {
        16...17 => 3,
        18 => 7,
        _ => 0,
    }
}

/// Calculate how many bits the huffman-encoded huffman lengths will use.
fn calculate_huffman_length(frequencies: &[FrequencyType], code_lengths: &[u8]) -> u64 {
    frequencies.iter().zip(code_lengths).enumerate().fold(
        0,
        |acc, (n, (&f, &l))| {
            acc +
                (u64::from(f) *
                     (u64::from(l) + u64::from(extra_bits_for_huffman_length_code(n as u8))))
        },
    )
}

/// Calculate how many bits data with the given frequencies will use when compressed with dynamic
/// code lengths (first return value) and static code lengths (second return value).
///
/// Parameters:
/// Frequencies, length of dynamic codes, and a function to get how many extra bits in addition
/// to the length of the huffman code the symbol will use.
fn calculate_block_length<F>(
    frequencies: &[FrequencyType],
    dyn_code_lengths: &[u8],
    get_num_extra_bits: &F,
) -> (u64, u64)
where
    F: Fn(usize) -> u64,
{
    // Length of data represented by dynamic codes.
    let mut d_ll_length = 0u64;
    // length of data represented by static codes.
    let mut s_ll_length = 0u64;

    let iter = frequencies
        .iter()
        .zip(dyn_code_lengths.iter().zip(FIXED_CODE_LENGTHS.iter()))
        .enumerate();

    // This could maybe be optimised a bit by splitting the iteration of codes using extra bits and
    // codes not using extra bits, but the extra complexity may not be worth it.
    for (c, (&f, (&l, &fl))) in iter {
        // Frequency
        let f = u64::from(f);
        // How many extra bits the current code number needs.
        let extra_bits_for_code = get_num_extra_bits(c);

        d_ll_length += f * (u64::from(l) + extra_bits_for_code);
        s_ll_length += f * (u64::from(fl) + extra_bits_for_code);

    }

    (d_ll_length, s_ll_length)
}

/// Get how extra padding bits after a block start header a stored block would use.
///
/// # Panics
/// Panics if `pending_bits > 8`
fn stored_padding(pending_bits: u8) -> u64 {
    assert!(pending_bits <= 8);
    let free_space = 8 - pending_bits;
    if free_space >= BLOCK_MARKER_LENGTH {
        // There is space in the current byte for the header.
        free_space - BLOCK_MARKER_LENGTH
    } else {
        // The header will require an extra byte.
        8 - (BLOCK_MARKER_LENGTH - free_space)
    }.into()
}

/// Calculate the number of bits storing the data in stored blocks will take up, excluding the
/// first block start code and potential padding bits. As stored blocks have a maximum length,
/// (as opposed to fixed and dynamic ones), multiple blocks may have to be utilised.
///
/// # Panics
/// Panics if `input_bytes` is 0.
fn stored_length(input_bytes: u64) -> u64 {
    // Check how many stored blocks these bytes would take up.
    // (Integer divison rounding up.)
    let num_blocks = (input_bytes
                          .checked_sub(1)
                          .expect("Underflow calculating stored block length!") /
                          MAX_STORED_BLOCK_LENGTH as u64) + 1;
    // The length will be the input length and the headers for each block. (Excluding the start
    // of block code for the first one)
    (input_bytes + (STORED_BLOCK_HEADER_LENGTH as u64 * num_blocks) + (num_blocks - 1)) * 8
}

pub enum BlockType {
    Stored,
    Fixed,
    Dynamic(DynamicBlockHeader),
}

/// A struct containing the different data needed to write the header for a dynamic block.
///
/// The code lengths are stored directly in the `HuffmanTable` struct.
/// TODO: Do the same for other things here.
pub struct DynamicBlockHeader {
    /// Length of the run-length encoding symbols.
    pub huffman_table_lengths: Vec<u8>,
    /// Number of lengths for values describing the huffman table that encodes the length values
    /// of the main huffman tables.
    pub used_hclens: usize,
}

/// Generate the lengths of the huffman codes we will be using, using the
/// frequency of the different symbols/lengths/distances, and determine what block type will give
/// the shortest representation.
/// TODO: This needs a test
pub fn gen_huffman_lengths(
    l_freqs: &[FrequencyType],
    d_freqs: &[FrequencyType],
    num_input_bytes: u64,
    pending_bits: u8,
    l_lengths: &mut [u8; 288],
    d_lengths: &mut [u8; 32],
    length_buffers: &mut LengthBuffers,
) -> BlockType {
    // Avoid corner cases and issues if this is called for an empty block.
    // For blocks this short, a fixed block will be the shortest.
    // TODO: Find the minimum value it's worth doing calculations for.
    if num_input_bytes <= 4 {
        return BlockType::Fixed;
    };

    let l_freqs = remove_trailing_zeroes(l_freqs, MIN_NUM_LITERALS_AND_LENGTHS);
    let d_freqs = remove_trailing_zeroes(d_freqs, MIN_NUM_DISTANCES);

    // The huffman spec allows us to exclude zeroes at the end of the
    // table of huffman lengths.
    // Since a frequency of 0 will give an huffman
    // length of 0. We strip off the trailing zeroes before even
    // generating the lengths to save some work.
    // There is however a minimum number of values we have to keep
    // according to the deflate spec.
    // TODO: We could probably compute some of this in parallel.
    huffman_lengths_from_frequency_m(
        l_freqs,
        MAX_CODE_LENGTH,
        &mut length_buffers.leaf_buf,
        l_lengths,
    );
    huffman_lengths_from_frequency_m(
        d_freqs,
        MAX_CODE_LENGTH,
        &mut length_buffers.leaf_buf,
        d_lengths,
    );


    let used_lengths = l_freqs.len();
    let used_distances = d_freqs.len();

    // Encode length values
    let mut freqs = [0u16; 19];
    encode_lengths_m(
        l_lengths[..used_lengths]
            .iter()
            .chain(&d_lengths[..used_distances]),
        &mut length_buffers.length_buf,
        &mut freqs,
    );

    // Create huffman lengths for the length/distance code lengths
    let mut huffman_table_lengths = vec![0; freqs.len()];
    huffman_lengths_from_frequency_m(
        &freqs,
        MAX_HUFFMAN_CODE_LENGTH,
        &mut length_buffers.leaf_buf,
        huffman_table_lengths.as_mut_slice(),
    );

    // Count how many of these lengths we use.
    let used_hclens = HUFFMAN_LENGTH_ORDER.len() -
        HUFFMAN_LENGTH_ORDER
            .iter()
            .rev()
            .take_while(|&&n| huffman_table_lengths[n as usize] == 0)
            .count();

    // There has to be at least 4 hclens, so if there isn't, something went wrong.
    debug_assert!(used_hclens >= 4);

    // Calculate how many bytes of space this block will take up with the different block types
    // (excluding the 3-bit block header since it's used in all block types).

    // Total length of the compressed literals/lengths.
    let (d_ll_length, s_ll_length) = calculate_block_length(l_freqs, l_lengths, &|c| {
        num_extra_bits_for_length_code(c.saturating_sub(LENGTH_BITS_START as usize) as u8).into()
    });

    // Total length of the compressed distances.
    let (d_dist_length, s_dist_length) = calculate_block_length(d_freqs, d_lengths, &|c| {
        num_extra_bits_for_distance_code(c as u8).into()
    });

    // Total length of the compressed huffman code lengths.
    let huff_table_length = calculate_huffman_length(&freqs, &huffman_table_lengths);

    // For dynamic blocks the huffman tables takes up some extra space.
    let dynamic_length = d_ll_length + d_dist_length + huff_table_length +
        (used_hclens as u64 * 3) + u64::from(HLIT_BITS) +
        u64::from(HDIST_BITS) + u64::from(HCLEN_BITS);

    // Static blocks don't have any extra header data.
    let static_length = s_ll_length + s_dist_length;

    // Calculate how many bits it will take to store the data in uncompressed (stored) block(s).
    let stored_length = stored_length(num_input_bytes) + stored_padding(pending_bits % 8);

    let used_length = cmp::min(cmp::min(dynamic_length, static_length), stored_length);

    // Check if the block is actually compressed. If using a dynamic block
    // increases the length of the block (for instance if the input data is mostly random or
    // already compressed), we want to output a stored(uncompressed) block instead to avoid wasting
    // space.
    if used_length == static_length {
        BlockType::Fixed
    } else if used_length == stored_length {
        BlockType::Stored
    } else {
        BlockType::Dynamic(DynamicBlockHeader {
            huffman_table_lengths: huffman_table_lengths,
            used_hclens: used_hclens,
        })
    }
}

/// Write the specified huffman lengths to the bit writer
pub fn write_huffman_lengths(
    header: &DynamicBlockHeader,
    huffman_table: &HuffmanTable,
    encoded_lengths: &[EncodedLength],
    writer: &mut LsbWriter,
) {
    // Ignore trailing zero lengths as allowed by the deflate spec.
    let (literal_len_lengths, distance_lengths) = huffman_table.get_lengths();
    let literal_len_lengths =
        remove_trailing_zeroes(literal_len_lengths, MIN_NUM_LITERALS_AND_LENGTHS);
    let distance_lengths = remove_trailing_zeroes(distance_lengths, MIN_NUM_DISTANCES);
    let huffman_table_lengths = &header.huffman_table_lengths;
    let used_hclens = header.used_hclens;

    assert!(literal_len_lengths.len() <= NUM_LITERALS_AND_LENGTHS);
    assert!(literal_len_lengths.len() >= MIN_NUM_LITERALS_AND_LENGTHS);
    assert!(distance_lengths.len() <= NUM_DISTANCE_CODES);
    assert!(distance_lengths.len() >= MIN_NUM_DISTANCES);

    // Number of length codes - 257.
    let hlit = (literal_len_lengths.len() - MIN_NUM_LITERALS_AND_LENGTHS) as u16;
    writer.write_bits(hlit, HLIT_BITS);
    // Number of distance codes - 1.
    let hdist = (distance_lengths.len() - MIN_NUM_DISTANCES) as u16;
    writer.write_bits(hdist, HDIST_BITS);

    // Number of huffman table lengths - 4.
    let hclen = used_hclens.saturating_sub(4);

    // Write HCLEN.
    // Casting to u16 is safe since the length can never be more than the length of
    // `HUFFMAN_LENGTH_ORDER` anyhow.
    writer.write_bits(hclen as u16, HCLEN_BITS);

    // Write the lengths for the huffman table describing the huffman table
    // Each length is 3 bits
    for n in &HUFFMAN_LENGTH_ORDER[..used_hclens] {
        writer.write_bits(huffman_table_lengths[usize::from(*n)] as u16, 3);
    }

    // Generate codes for the main huffman table using the lengths we just wrote
    let mut codes = [0u16; NUM_HUFFMAN_LENGTHS];
    create_codes_in_place(&mut codes[..], huffman_table_lengths);

    // Write the actual huffman lengths
    for v in encoded_lengths {
        match *v {
            EncodedLength::Length(n) => {
                let (c, l) = (codes[usize::from(n)], huffman_table_lengths[usize::from(n)]);
                writer.write_bits(c, l);
            }
            EncodedLength::CopyPrevious(n) => {
                let (c, l) = (codes[COPY_PREVIOUS], huffman_table_lengths[COPY_PREVIOUS]);
                writer.write_bits(c, l);
                debug_assert!(n >= 3);
                debug_assert!(n <= 6);
                writer.write_bits((n - 3).into(), 2);
            }
            EncodedLength::RepeatZero3Bits(n) => {
                let (c, l) = (
                    codes[REPEAT_ZERO_3_BITS],
                    huffman_table_lengths[REPEAT_ZERO_3_BITS],
                );
                writer.write_bits(c, l);
                debug_assert!(n >= 3);
                writer.write_bits((n - 3).into(), 3);
            }
            EncodedLength::RepeatZero7Bits(n) => {
                let (c, l) = (
                    codes[REPEAT_ZERO_7_BITS],
                    huffman_table_lengths[REPEAT_ZERO_7_BITS],
                );
                writer.write_bits(c, l);
                debug_assert!(n >= 11);
                debug_assert!(n <= 138);
                writer.write_bits((n - 11).into(), 7);
            }
        }
    }
}


#[cfg(test)]
mod test {
    use super::stored_padding;
    #[test]
    fn padding() {
        assert_eq!(stored_padding(0), 5);
        assert_eq!(stored_padding(1), 4);
        assert_eq!(stored_padding(2), 3);
        assert_eq!(stored_padding(3), 2);
        assert_eq!(stored_padding(4), 1);
        assert_eq!(stored_padding(5), 0);
        assert_eq!(stored_padding(6), 7);
        assert_eq!(stored_padding(7), 6);
    }
}
