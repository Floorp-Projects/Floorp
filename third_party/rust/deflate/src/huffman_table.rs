use std::fmt;
use bit_reverse::reverse_bits;
use lzvalue::StoredLength;

// The number of length codes in the huffman table
pub const NUM_LENGTH_CODES: usize = 29;

// The number of distance codes in the distance huffman table
// NOTE: two mode codes are actually used when constructing codes
pub const NUM_DISTANCE_CODES: usize = 30;

// Combined number of literal and length codes
// NOTE: two mode codes are actually used when constructing codes
pub const NUM_LITERALS_AND_LENGTHS: usize = 286;


// The maximum length of a huffman code
pub const MAX_CODE_LENGTH: usize = 15;

// The minimun and maximum lengths for a match according to the DEFLATE specification
pub const MIN_MATCH: u16 = 3;
pub const MAX_MATCH: u16 = 258;

pub const MIN_DISTANCE: u16 = 1;
pub const MAX_DISTANCE: u16 = 32768;


// The position in the literal/length table of the end of block symbol
pub const END_OF_BLOCK_POSITION: usize = 256;

// Bit lengths for literal and length codes in the fixed huffman table
// The huffman codes are generated from this and the distance bit length table
pub static FIXED_CODE_LENGTHS: [u8; NUM_LITERALS_AND_LENGTHS + 2] = [
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
];



// The number of extra bits for the length codes
const LENGTH_EXTRA_BITS_LENGTH: [u8; NUM_LENGTH_CODES] = [
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    3,
    3,
    3,
    3,
    4,
    4,
    4,
    4,
    5,
    5,
    5,
    5,
    0,
];

// Table used to get a code from a length value (see get_distance_code_and_extra_bits)
const LENGTH_CODE: [u8; 256] = [
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    8,
    9,
    9,
    10,
    10,
    11,
    11,
    12,
    12,
    12,
    12,
    13,
    13,
    13,
    13,
    14,
    14,
    14,
    14,
    15,
    15,
    15,
    15,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    17,
    17,
    17,
    17,
    17,
    17,
    17,
    17,
    18,
    18,
    18,
    18,
    18,
    18,
    18,
    18,
    19,
    19,
    19,
    19,
    19,
    19,
    19,
    19,
    20,
    20,
    20,
    20,
    20,
    20,
    20,
    20,
    20,
    20,
    20,
    20,
    20,
    20,
    20,
    20,
    21,
    21,
    21,
    21,
    21,
    21,
    21,
    21,
    21,
    21,
    21,
    21,
    21,
    21,
    21,
    21,
    22,
    22,
    22,
    22,
    22,
    22,
    22,
    22,
    22,
    22,
    22,
    22,
    22,
    22,
    22,
    22,
    23,
    23,
    23,
    23,
    23,
    23,
    23,
    23,
    23,
    23,
    23,
    23,
    23,
    23,
    23,
    23,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    28,
];

// Base values to calculate the value of the bits in length codes
const BASE_LENGTH: [u8; NUM_LENGTH_CODES] = [
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    10,
    12,
    14,
    16,
    20,
    24,
    28,
    32,
    40,
    48,
    56,
    64,
    80,
    96,
    112,
    128,
    160,
    192,
    224,
    255,
]; // 258 - MIN_MATCh

// What number in the literal/length table the lengths start at
pub const LENGTH_BITS_START: u16 = 257;

// Lengths for the distance codes in the pre-defined/fixed huffman table
// (All distance codes are 5 bits long)
pub const FIXED_CODE_LENGTHS_DISTANCE: [u8; NUM_DISTANCE_CODES + 2] = [5; NUM_DISTANCE_CODES + 2];

const DISTANCE_CODES: [u8; 512] = [
    0,
    1,
    2,
    3,
    4,
    4,
    5,
    5,
    6,
    6,
    6,
    6,
    7,
    7,
    7,
    7,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    10,
    10,
    10,
    10,
    10,
    10,
    10,
    10,
    10,
    10,
    10,
    10,
    10,
    10,
    10,
    10,
    11,
    11,
    11,
    11,
    11,
    11,
    11,
    11,
    11,
    11,
    11,
    11,
    11,
    11,
    11,
    11,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    13,
    13,
    13,
    13,
    13,
    13,
    13,
    13,
    13,
    13,
    13,
    13,
    13,
    13,
    13,
    13,
    13,
    13,
    13,
    13,
    13,
    13,
    13,
    13,
    13,
    13,
    13,
    13,
    13,
    13,
    13,
    13,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    14,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    0,
    0,
    16,
    17,
    18,
    18,
    19,
    19,
    20,
    20,
    20,
    20,
    21,
    21,
    21,
    21,
    22,
    22,
    22,
    22,
    22,
    22,
    22,
    22,
    23,
    23,
    23,
    23,
    23,
    23,
    23,
    23,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    25,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    26,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    27,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    28,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
    29,
];

// Number of extra bits following the distance codes
#[cfg(test)]
const DISTANCE_EXTRA_BITS: [u8; NUM_DISTANCE_CODES] = [
    0,
    0,
    0,
    0,
    1,
    1,
    2,
    2,
    3,
    3,
    4,
    4,
    5,
    5,
    6,
    6,
    7,
    7,
    8,
    8,
    9,
    9,
    10,
    10,
    11,
    11,
    12,
    12,
    13,
    13,
];

const DISTANCE_BASE: [u16; NUM_DISTANCE_CODES] = [
    0,
    1,
    2,
    3,
    4,
    6,
    8,
    12,
    16,
    24,
    32,
    48,
    64,
    96,
    128,
    192,
    256,
    384,
    512,
    768,
    1024,
    1536,
    2048,
    3072,
    4096,
    6144,
    8192,
    12288,
    16384,
    24576,
];

pub fn num_extra_bits_for_length_code(code: u8) -> u8 {
    LENGTH_EXTRA_BITS_LENGTH[code as usize]
}

/// Get the number of extra bits used for a distance code.
/// (Code numbers above `NUM_DISTANCE_CODES` will give some garbage
/// value.)
pub fn num_extra_bits_for_distance_code(code: u8) -> u8 {
    // This can be easily calculated without a lookup.
    //
    let mut c = code >> 1;
    c -= (c != 0) as u8;
    c
}

/// A struct representing the data needed to generate the bit codes for
/// a given value and huffman table.
#[derive(Copy, Clone)]
struct ExtraBits {
    // The position of the length in the huffman table.
    pub code_number: u16,
    // Number of extra bits following the code.
    pub num_bits: u8,
    // The value of the extra bits, which together with the length/distance code
    // allow us to calculate the exact length/distance.
    pub value: u16,
}

/// Get the length code that corresponds to the length value
/// Panics if length is out of range.
pub fn get_length_code(length: u16) -> usize {
    // Going via an u8 here helps the compiler evade bounds checking.
    usize::from(LENGTH_CODE[(length.wrapping_sub(MIN_MATCH)) as u8 as usize]) +
        LENGTH_BITS_START as usize
}

/// Get the code for the huffman table and the extra bits for the requested length.
fn get_length_code_and_extra_bits(length: StoredLength) -> ExtraBits {
    // Length values are stored as unsigned bytes, where the actual length is the value - 3
    // The `StoredLength` struct takes care of this conversion for us.
    let n = LENGTH_CODE[length.stored_length() as usize];

    // We can then get the base length from the base length table,
    // which we use to calculate the value of the extra bits.
    let base = BASE_LENGTH[n as usize];
    let num_bits = num_extra_bits_for_length_code(n);
    ExtraBits {
        code_number: u16::from(n) + LENGTH_BITS_START,
        num_bits: num_bits,
        value: (length.stored_length() - base).into(),
    }

}

/// Get the spot in the huffman table for distances `distance` corresponds to
/// Returns 255 if the distance is invalid.
/// Avoiding option here for simplicity and performance) as this being called with an invalid
/// value would be a bug.
pub fn get_distance_code(distance: u16) -> u8 {
    let distance = distance as usize;

    match distance {
        // Since the array starts at 0, we need to subtract 1 to get the correct code number.
        1...256 => DISTANCE_CODES[distance - 1],
        // Due to the distrubution of the distance codes above 256, we can get away with only
        // using the top bits to determine the code, rather than having a 32k long table of
        // distance codes.
        257...32768 => DISTANCE_CODES[256 + ((distance - 1) >> 7)],
        _ => 0,
    }
}


fn get_distance_code_and_extra_bits(distance: u16) -> ExtraBits {
    let distance_code = get_distance_code(distance);
    let extra = num_extra_bits_for_distance_code(distance_code);
    // FIXME: We should add 1 to the values in distance_base to avoid having to add one here
    let base = DISTANCE_BASE[distance_code as usize] + 1;
    ExtraBits {
        code_number: distance_code.into(),
        num_bits: extra,
        value: distance - base,
    }
}

#[derive(Copy, Clone, Default)]
pub struct HuffmanCode {
    pub code: u16,
    pub length: u8,
}

impl fmt::Debug for HuffmanCode {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "HuffmanCode {{ code: {:b}, length: {}}}",
            self.code,
            self.length
        )
    }
}

impl HuffmanCode {
    #[inline]
    /// Create a huffman code value from a code and length.
    fn new(code: u16, length: u8) -> HuffmanCode {
        HuffmanCode {
            code: code,
            length: length,
        }
    }
}

#[cfg(test)]
pub struct LengthAndDistanceBits {
    pub length_code: HuffmanCode,
    pub length_extra_bits: HuffmanCode,
    pub distance_code: HuffmanCode,
    pub distance_extra_bits: HuffmanCode,
}

/// Counts the number of values of each length.
/// Returns a tuple containing the longest length value in the table, it's position,
/// and fills in lengths in the `len_counts` slice.
/// Returns an error if `table` is empty, or if any of the lengths exceed 15.
fn build_length_count_table(table: &[u8], len_counts: &mut [u16; 16]) -> (usize, usize) {
    // TODO: Validate the length table properly in debug mode.
    let max_length = (*table.iter().max().expect("BUG! Empty lengths!")).into();

    assert!(max_length <= MAX_CODE_LENGTH);

    let mut max_length_pos = 0;

    for (n, &length) in table.iter().enumerate() {
        // TODO: Make sure we don't have more of one length than we can make
        // codes for
        if length > 0 {
            len_counts[usize::from(length)] += 1;
            max_length_pos = n;
        }
    }
    (max_length, max_length_pos)
}

/// Generats a vector of huffman codes given a table of bit lengths
/// Returns an error if any of the lengths are > 15
pub fn create_codes_in_place(code_table: &mut [u16], length_table: &[u8]) {
    let mut len_counts = [0; 16];
    let (max_length, max_length_pos) = build_length_count_table(length_table, &mut len_counts);
    let lengths = len_counts;

    let mut code = 0u16;
    let mut next_code = Vec::with_capacity(length_table.len());
    next_code.push(code);

    for bits in 1..max_length + 1 {
        code = (code + lengths[bits - 1]) << 1;
        next_code.push(code);
    }

    for n in 0..max_length_pos + 1 {
        let length = usize::from(length_table[n]);
        if length != 0 {
            // The algorithm generates the code in the reverse bit order, so we need to reverse them
            // to get the correct codes.
            code_table[n] = reverse_bits(next_code[length], length as u8);
            // We use wrapping here as we would otherwise overflow on the last code
            // This should be okay as we exit the loop after this so the value is ignored
            next_code[length] = next_code[length].wrapping_add(1);
        }
    }
}

/// A structure containing the tables of huffman codes for lengths, literals and distances
pub struct HuffmanTable {
    // Literal, end of block and length codes
    codes: [u16; 288],
    code_lengths: [u8; 288],
    // Distance codes
    distance_codes: [u16; 32],
    distance_code_lengths: [u8; 32],
}

impl HuffmanTable {
    pub fn empty() -> HuffmanTable {
        HuffmanTable {
            codes: [0; 288],
            code_lengths: [0; 288],
            distance_codes: [0; 32],
            distance_code_lengths: [0; 32],
        }
    }

    #[cfg(test)]
    pub fn from_length_tables(
        literals_and_lengths: &[u8; 288],
        distances: &[u8; 32],
    ) -> HuffmanTable {
        let mut table = HuffmanTable {
            codes: [0; 288],
            code_lengths: *literals_and_lengths,
            distance_codes: [0; 32],
            distance_code_lengths: *distances,
        };

        table.update_from_lengths();
        table
    }

    /// Get references to the lenghts of the current huffman codes.
    #[inline]
    pub fn get_lengths(&self) -> (&[u8; 288], &[u8; 32]) {
        (&self.code_lengths, &self.distance_code_lengths)
    }

    /// Get mutable references to the lenghts of the current huffman codes.
    ///
    /// Used for updating the lengths in place.
    #[inline]
    pub fn get_lengths_mut(&mut self) -> (&mut [u8; 288], &mut [u8; 32]) {
        (&mut self.code_lengths, &mut self.distance_code_lengths)
    }

    /// Update the huffman codes using the existing length values in the huffman table.
    pub fn update_from_lengths(&mut self) {
        create_codes_in_place(self.codes.as_mut(), &self.code_lengths[..]);
        create_codes_in_place(
            self.distance_codes.as_mut(),
            &self.distance_code_lengths[..],
        );
    }

    pub fn set_to_fixed(&mut self) {
        self.code_lengths = FIXED_CODE_LENGTHS;
        self.distance_code_lengths = FIXED_CODE_LENGTHS_DISTANCE;
        self.update_from_lengths();
    }

    /// Create a HuffmanTable using the fixed tables specified in the DEFLATE format specification.
    #[cfg(test)]
    pub fn fixed_table() -> HuffmanTable {
        // This should be safe to unwrap, if it were to panic the code is wrong,
        // tests should catch it.
        HuffmanTable::from_length_tables(&FIXED_CODE_LENGTHS, &FIXED_CODE_LENGTHS_DISTANCE)
    }

    #[inline]
    fn get_ll_huff(&self, value: usize) -> HuffmanCode {
        HuffmanCode::new(self.codes[value], self.code_lengths[value])
    }

    /// Get the huffman code from the corresponding literal value
    #[inline]
    pub fn get_literal(&self, value: u8) -> HuffmanCode {
        let index = usize::from(value);
        HuffmanCode::new(self.codes[index], self.code_lengths[index])
    }

    /// Get the huffman code for the end of block value
    #[inline]
    pub fn get_end_of_block(&self) -> HuffmanCode {
        self.get_ll_huff(END_OF_BLOCK_POSITION)
    }

    /// Get the huffman code and extra bits for the specified length
    #[inline]
    pub fn get_length_huffman(&self, length: StoredLength) -> (HuffmanCode, HuffmanCode) {

        let length_data = get_length_code_and_extra_bits(length);

        let length_huffman_code = self.get_ll_huff(length_data.code_number as usize);

        (
            length_huffman_code,
            HuffmanCode {
                code: length_data.value,
                length: length_data.num_bits,
            },
        )
    }

    /// Get the huffman code and extra bits for the specified distance
    ///
    /// Returns None if distance is 0 or above 32768
    #[inline]
    pub fn get_distance_huffman(&self, distance: u16) -> (HuffmanCode, HuffmanCode) {
        debug_assert!(distance >= MIN_DISTANCE && distance <= MAX_DISTANCE);

        let distance_data = get_distance_code_and_extra_bits(distance);

        let distance_huffman_code = self.distance_codes[distance_data.code_number as usize];
        let distance_huffman_length =
            self.distance_code_lengths[distance_data.code_number as usize];

        (
            HuffmanCode {
                code: distance_huffman_code,
                length: distance_huffman_length,
            },
            HuffmanCode {
                code: distance_data.value,
                length: distance_data.num_bits,
            },
        )
    }

    #[cfg(test)]
    pub fn get_length_distance_code(&self, length: u16, distance: u16) -> LengthAndDistanceBits {
        assert!(length >= MIN_MATCH && length < MAX_DISTANCE);
        let l_codes = self.get_length_huffman(StoredLength::from_actual_length(length));
        let d_codes = self.get_distance_huffman(distance);
        LengthAndDistanceBits {
            length_code: l_codes.0,
            length_extra_bits: l_codes.1,
            distance_code: d_codes.0,
            distance_extra_bits: d_codes.1,
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use super::{get_length_code_and_extra_bits, get_distance_code_and_extra_bits,
                build_length_count_table};

    use lzvalue::StoredLength;

    fn l(length: u16) -> StoredLength {
        StoredLength::from_actual_length(length)
    }

    #[test]
    fn test_get_length_code() {
        let extra_bits = get_length_code_and_extra_bits(l(4));
        assert_eq!(extra_bits.code_number, 258);
        assert_eq!(extra_bits.num_bits, 0);
        assert_eq!(extra_bits.value, 0);

        let extra_bits = get_length_code_and_extra_bits(l(165));
        assert_eq!(extra_bits.code_number, 282);
        assert_eq!(extra_bits.num_bits, 5);
        assert_eq!(extra_bits.value, 2);

        let extra_bits = get_length_code_and_extra_bits(l(257));
        assert_eq!(extra_bits.code_number, 284);
        assert_eq!(extra_bits.num_bits, 5);
        assert_eq!(extra_bits.value, 30);

        let extra_bits = get_length_code_and_extra_bits(l(258));
        assert_eq!(extra_bits.code_number, 285);
        assert_eq!(extra_bits.num_bits, 0);
    }

    #[test]
    fn test_distance_code() {
        assert_eq!(get_distance_code(1), 0);
        // Using 0 for None at the moment
        assert_eq!(get_distance_code(0), 0);
        assert_eq!(get_distance_code(50000), 0);
        assert_eq!(get_distance_code(6146), 25);
        assert_eq!(get_distance_code(256), 15);
        assert_eq!(get_distance_code(4733), 24);
        assert_eq!(get_distance_code(257), 16);
    }

    #[test]
    fn test_distance_extra_bits() {
        let extra = get_distance_code_and_extra_bits(527);
        assert_eq!(extra.value, 0b1110);
        assert_eq!(extra.code_number, 18);
        assert_eq!(extra.num_bits, 8);
        let extra = get_distance_code_and_extra_bits(256);
        assert_eq!(extra.code_number, 15);
        assert_eq!(extra.num_bits, 6);
        let extra = get_distance_code_and_extra_bits(4733);
        assert_eq!(extra.code_number, 24);
        assert_eq!(extra.num_bits, 11);
    }

    #[test]
    fn test_length_table_fixed() {
        let _ = build_length_count_table(&FIXED_CODE_LENGTHS, &mut [0; 16]);
    }

    #[test]
    #[should_panic]
    fn test_length_table_max_length() {
        let table = [16u8; 288];
        build_length_count_table(&table, &mut [0; 16]);
    }

    #[test]
    #[should_panic]
    fn test_empty_table() {
        let table = [];
        build_length_count_table(&table, &mut [0; 16]);
    }

    #[test]
    fn make_table_fixed() {
        let table = HuffmanTable::fixed_table();
        assert_eq!(table.codes[0], 0b00001100);
        assert_eq!(table.codes[143], 0b11111101);
        assert_eq!(table.codes[144], 0b000010011);
        assert_eq!(table.codes[255], 0b111111111);
        assert_eq!(table.codes[256], 0b0000000);
        assert_eq!(table.codes[279], 0b1110100);
        assert_eq!(table.codes[280], 0b00000011);
        assert_eq!(table.codes[287], 0b11100011);

        assert_eq!(table.distance_codes[0], 0);
        assert_eq!(table.distance_codes[5], 20);

        let ld = table.get_length_distance_code(4, 5);

        assert_eq!(ld.length_code.code, 0b00100000);
        assert_eq!(ld.distance_code.code, 0b00100);
        assert_eq!(ld.distance_extra_bits.length, 1);
        assert_eq!(ld.distance_extra_bits.code, 0);
    }

    #[test]
    fn extra_bits_distance() {
        use std::mem::size_of;
        for i in 0..NUM_DISTANCE_CODES {
            assert_eq!(
                num_extra_bits_for_distance_code(i as u8),
                DISTANCE_EXTRA_BITS[i]
            );
        }
        println!("Size of huffmanCode struct: {}", size_of::<HuffmanCode>());
    }

}
