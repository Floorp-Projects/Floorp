//! Streaming decompression functionality.

use super::*;
use shared::{HUFFMAN_LENGTH_ORDER, update_adler32};

use std::{cmp, ptr, slice};

use self::output_buffer::OutputBuffer;

pub const TINFL_LZ_DICT_SIZE: usize = 32_768;

/// A struct containing huffman code lengths and the huffman code tree used by the decompressor.
#[repr(C)]
struct HuffmanTable {
    /// Length of the code at each index.
    pub code_size: [u8; 288],
    /// Fast lookup table for shorter huffman codes.
    ///
    /// See `HuffmanTable::fast_lookup`.
    pub look_up: [i16; 1024],
    /// Full huffman tree.
    ///
    /// Positive values are edge nodes/symbols, negative values are
    /// parent nodes/references to other nodes.
    pub tree: [i16; 576],
}

impl HuffmanTable {
    fn new() -> HuffmanTable {
        HuffmanTable {
            code_size: [0; 288],
            look_up: [0; 1024],
            tree: [0; 576],
        }
    }

    /// Look for a symbol in the fast lookup table.
    /// The symbol is stored in the lower 9 bits, the length in the next 6.
    /// If the returned value is negative, the code wasn't found in the
    /// fast lookup table and the full tree has to be traversed to find the code.
    #[inline]
    fn fast_lookup(&self, bit_buf: BitBuffer) -> i16 {
        self.look_up[(bit_buf & (FAST_LOOKUP_SIZE - 1) as BitBuffer) as usize]
    }

    /// Get the symbol and the code length from the huffman tree.
    #[inline]
    fn tree_lookup(&self, fast_symbol: i32, bit_buf: BitBuffer, mut code_len: u32) -> (i32, u32) {
        let mut symbol = fast_symbol;
        // We step through the tree until we encounter a positive value, which indicates a
        // symbol.
        loop {
            // symbol here indicates the position of the left (0) node, if the next bit is 1
            // we add 1 to the lookup position to get the right node.
            symbol = self.tree[(!symbol + ((bit_buf >> code_len) & 1) as i32) as usize] as i32;
            code_len += 1;
            if symbol >= 0 {
                break;
            }
        }
        (symbol, code_len)
    }

    #[inline]
    /// Look up a symbol and code length from the bits in the provided bit buffer.
    fn lookup(&self, bit_buf: BitBuffer) -> (i32, u32) {
        let symbol = self.fast_lookup(bit_buf).into();
        if symbol >= 0 {
            (symbol, (symbol >> 9) as u32)
        } else {
            // We didn't get a symbol from the fast lookup table, so check the tree instead.
            self.tree_lookup(symbol.into(), bit_buf, FAST_LOOKUP_BITS.into())
        }
    }
}

/// The number of huffman tables used.
const MAX_HUFF_TABLES: usize = 3;
/// The length of the first (literal/length) huffman table.
const MAX_HUFF_SYMBOLS_0: usize = 288;
/// The length of the second (distance) huffman table.
const MAX_HUFF_SYMBOLS_1: usize = 32;
/// The length of the last (huffman code length) huffman table.
const _MAX_HUFF_SYMBOLS_2: usize = 19;
/// The maximum length of a code that can be looked up in the fast lookup table.
const FAST_LOOKUP_BITS: u8 = 10;
/// The size of the fast lookup table.
const FAST_LOOKUP_SIZE: u32 = 1 << FAST_LOOKUP_BITS;
const LITLEN_TABLE: usize = 0;
const DIST_TABLE: usize = 1;
const HUFFLEN_TABLE: usize = 2;


pub mod inflate_flags {
    /// Should we try to parse a zlib header?
    pub const TINFL_FLAG_PARSE_ZLIB_HEADER: u32 = 1;
    /// There is more input that hasn't been given to the decompressor yet.
    pub const TINFL_FLAG_HAS_MORE_INPUT: u32 = 2;
    /// The output buffer should not wrap around.
    pub const TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF: u32 = 4;
    /// Should we calculate the adler32 checksum of the output data?
    pub const TINFL_FLAG_COMPUTE_ADLER32: u32 = 8;
}

use self::inflate_flags::*;

const MIN_TABLE_SIZES: [u16; 3] = [257, 1, 4];

#[cfg(target_pointer_width = "64")]
type BitBuffer = u64;

#[cfg(not(target_pointer_width = "64"))]
type BitBuffer = u32;

/// Main decompression struct.
///
/// This is repr(C) to be usable in the C API.
#[repr(C)]
#[allow(bad_style)]
pub struct DecompressorOxide {
    /// Current state of the decompressor.
    state: core::State,
    /// Number of bits in the bit buffer.
    num_bits: u32,
    /// Zlib CMF
    z_header0: u32,
    /// Zlib FLG
    z_header1: u32,
    /// Adler32 checksum from the zlib header.
    z_adler32: u32,
    /// 1 if the current block is the last block, 0 otherwise.
    finish: u32,
    /// The type of the current block.
    block_type: u32,
    /// 1 if the adler32 value should be checked.
    check_adler32: u32,
    /// Last match distance.
    dist: u32,
    /// Variable used for match length, symbols, and a number of other things.
    counter: u32,
    /// Number of extra bits for the last length or distance code.
    num_extra: u32,
    /// Number of entries in each huffman table.
    table_sizes: [u32; MAX_HUFF_TABLES],
    /// Buffer of input data.
    bit_buf: BitBuffer,
    /// Position in the output buffer.
    dist_from_out_buf_start: usize,
    /// Huffman tables.
    tables: [HuffmanTable; MAX_HUFF_TABLES],
    /// Raw block header.
    raw_header: [u8; 4],
    /// Huffman length codes.
    len_codes: [u8; MAX_HUFF_SYMBOLS_0 + MAX_HUFF_SYMBOLS_1 + 137],
}

impl DecompressorOxide {
    /// Create a new tinfl_decompressor with all fields set to 0.
    pub fn new() -> DecompressorOxide {
        DecompressorOxide::default()
    }

    /// Create a new tinfl_decompressor with all fields set to 0.
    pub fn default() -> DecompressorOxide {
        DecompressorOxide {
            state: core::State::Start,
            num_bits: 0,
            z_header0: 0,
            z_header1: 0,
            z_adler32: 0,
            finish: 0,
            block_type: 0,
            check_adler32: 0,
            dist: 0,
            counter: 0,
            num_extra: 0,
            table_sizes: [0; MAX_HUFF_TABLES],
            bit_buf: 0,
            dist_from_out_buf_start: 0,
            // TODO:(oyvindln) Check that copies here are optimized out in release mode.
            tables: [HuffmanTable::new(), HuffmanTable::new(), HuffmanTable::new()],
            raw_header: [0; 4],
            len_codes: [0; MAX_HUFF_SYMBOLS_0 + MAX_HUFF_SYMBOLS_1 + 137],
        }
    }

    /// Set the current state to `Start`.
    #[inline]
    pub fn init(&mut self) {
        self.state = core::State::Start;
    }

    /// Create a new decompressor with only the state field initialized.
    ///
    /// This is how it's created in miniz. Unsafe due to uninitialized values.
    #[inline]
    pub unsafe fn with_init_state_only() -> DecompressorOxide {
        let mut decomp: DecompressorOxide = mem::uninitialized();
        decomp.state = core::State::Start;
        decomp
    }

    /// Returns the adler32 checksum of the currently decompressed data.
    #[inline]
    pub fn adler32(&self) -> Option<u32> {
        if self.state != core::State::Start &&
            self.state != core::State::BadZlibHeader && self.z_header0 != 0
            {
                Some(self.check_adler32)
            } else {
            None
        }
    }
}

#[derive(Copy, Clone, PartialEq, Eq, Debug)]
#[repr(C)]
enum State {
    Start = 0,
    ReadZlibCmf,
    ReadZlibFlg,
    ReadBlockHeader,
    BlockTypeNoCompression,
    RawHeader,
    RawMemcpy1,
    RawMemcpy2,
    ReadTableSizes,
    ReadHufflenTableCodeSize,
    ReadLitlenDistTablesCodeSize,
    ReadExtraBitsCodeSize,
    DecodeLitlen,
    WriteSymbol,
    ReadExtraBitsLitlen,
    DecodeDistance,
    ReadExtraBitsDistance,
    RawReadFirstByte,
    RawStoreFirstByte,
    WriteLenBytesToEnd,
    BlockDone,
    HuffDecodeOuterLoop1,
    HuffDecodeOuterLoop2,
    ReadAdler32,

    DoneForever,

    // Failure states.
    BlockTypeUnexpected,
    BadCodeSizeSum,
    BadTotalSymbols,
    BadZlibHeader,
    DistanceOutOfBounds,
    BadRawLength,
    BadCodeSizeDistPrevLookup,
}

impl State {
    #[inline]
    fn begin(&mut self, new_state: State) {
        *self = new_state;
    }
}

use self::State::*;

// Not sure why miniz uses 32-bit values for these, maybe alignment/cache again?
// # Optimization
// We add a extra zero value at the end and make the tables 32 elements long
// so we can use a mask to avoid bounds checks.
/// Base length for each length code.
///
/// The base is used together with the value of the extra bits to decode the actual
/// length/distance values in a match.
#[cfg_attr(rustfmt, rustfmt_skip)]
const LENGTH_BASE: [u16; 32] = [
    3,  4,  5,  6,  7,  8,  9,  10,  11,  13,  15,  17,  19,  23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0,  0, 0
];

/// Number of extra bits for each length code.
#[cfg_attr(rustfmt, rustfmt_skip)]
const LENGTH_EXTRA: [u8; 32] = [
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0, 0, 0, 0
];

/// Base length for each distance code.
#[cfg_attr(rustfmt, rustfmt_skip)]
const DIST_BASE: [u16; 32] = [
    1,    2,    3,    4,    5,    7,      9,      13,     17,  25,   33,
    49,   65,   97,   129,  193,  257,    385,    513,    769, 1025, 1537,
    2049, 3073, 4097, 6145, 8193, 12_289, 16_385, 24_577, 0,   0
];

/// Number of extra bits for each distance code.
#[cfg_attr(rustfmt, rustfmt_skip)]
const DIST_EXTRA: [u8; 32] = [
    0, 0, 0, 0, 1, 1, 2,  2,  3,  3,  4,  4,  5,  5,  6,  6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 13, 13
];

/// The mask used when indexing the base/extra arrays.
const BASE_EXTRA_MASK: usize = 32 - 1;

/// Sets the value of all the elements of the slice to `val`.
#[inline]
fn memset<T: Copy>(slice: &mut [T], val: T) {
    for x in slice {
        *x = val
    }
}

/// Read an le u16 value from the slice iterator.
///
/// # Panics
/// Panics if there are less than two bytes left.
#[inline]
#[cfg(not(target_pointer_width = "64"))]
fn read_u16_le(iter: &mut slice::Iter<u8>) -> u16 {
    let ret = {
        let two_bytes = &iter.as_ref()[0..2];
        // # Unsafe
        //
        // The slice was just bounds checked to be 2 bytes long.
        unsafe { ptr::read_unaligned(two_bytes.as_ptr() as *const u16) }
    };
    iter.nth(1);
    u16::from_le(ret)
}

/// Read an le u32 value from the slice iterator.
///
/// # Panics
/// Panics if there are less than four bytes left.
#[inline(always)]
#[cfg(target_pointer_width = "64")]
fn read_u32_le(iter: &mut slice::Iter<u8>) -> u32 {
    let ret = {
        let four_bytes = &iter.as_ref()[..4];
        // # Unsafe
        //
        // The slice was just bounds checked to be 4 bytes long.
        unsafe { ptr::read_unaligned(four_bytes.as_ptr() as *const u32) }
    };
    iter.nth(3);
    u32::from_le(ret)
}

/// Ensure that there is data in the bit buffer.
///
/// On 64-bit platform, we use a 64-bit value so this will
/// result in there being at least 32 bits in the bit buffer.
/// This function assumes that there is at least 4 bytes left in the input buffer.
#[inline(always)]
#[cfg(target_pointer_width = "64")]
fn fill_bit_buffer(l: &mut LocalVars, in_iter: &mut slice::Iter<u8>) {
    // Read four bytes into the buffer at once.
    if l.num_bits < 30 {
        l.bit_buf |= (read_u32_le(in_iter) as BitBuffer) << l.num_bits;
        l.num_bits += 32;
    }
}

/// Same as previous, but for non-64-bit platforms.
/// Ensures at least 16 bits are present, requires at least 2 bytes in the in buffer.
#[inline(always)]
#[cfg(not(target_pointer_width = "64"))]
fn fill_bit_buffer(l: &mut LocalVars, in_iter: &mut slice::Iter<u8>) {
    // If the buffer is 32-bit wide, read 2 bytes instead.
    if l.num_bits < 15 {
        l.bit_buf |= (read_u16_le(in_iter) as BitBuffer) << l.num_bits;
        l.num_bits += 16;
    }
}

#[inline]
fn _transfer_unaligned_u64(buf: &mut &mut [u8], from: isize, to: isize) {
    unsafe {
        let mut data = ptr::read_unaligned((*buf).as_ptr().offset(from) as *const u32);
        ptr::write_unaligned((*buf).as_mut_ptr().offset(to) as *mut u32, data);

        data = ptr::read_unaligned((*buf).as_ptr().offset(from + 4) as *const u32);
        ptr::write_unaligned((*buf).as_mut_ptr().offset(to + 4) as *mut u32, data);
    };
}

/// Check that the zlib header is correct and that there is enough space in the buffer
/// for the window size specified in the header.
///
/// See https://tools.ietf.org/html/rfc1950
#[inline]
fn validate_zlib_header(cmf: u32, flg: u32, flags: u32, mask: usize) -> Action {
    let mut failed =
    // cmf + flg should be divisible by 31.
        (((cmf * 256) + flg) % 31 != 0) ||
    // If this flag is set, a dictionary was used for this zlib compressed data.
    // This is currently not supported by miniz or miniz-oxide
        ((flg & 0b0010_0000) != 0) ||
    // Compression method. Only 8(DEFLATE) is defined by the standard.
        ((cmf & 15) != 8);

    let window_size = 1 << ((cmf >> 4) + 8);
    if (flags & TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF) == 0 {
        // Bail if the buffer is wrapping and the window size is larger than the buffer.
        failed |= (mask + 1) < window_size;
    }

    // Zlib doesn't allow window sizes above 32 * 1024.
    failed |= window_size > 32_768;

    if failed {
        Action::Jump(BadZlibHeader)
    } else {
        Action::Jump(ReadBlockHeader)
    }
}


enum Action {
    None,
    Jump(State),
    End(TINFLStatus),
}

/// Try to decode the next huffman code, and puts it in the counter field of the decompressor
/// if successful.
///
/// # Returns
/// The specified action returned from `f` on success,
/// `Action::End` if there are not enough data left to decode a symbol.
fn decode_huffman_code<F>(
    r: &mut DecompressorOxide,
    l: &mut LocalVars,
    table: usize,
    flags: u32,
    in_iter: &mut slice::Iter<u8>,
    f: F,
) -> Action
where
    F: FnOnce(&mut DecompressorOxide, &mut LocalVars, i32) -> Action,
{
    // As the huffman codes can be up to 15 bits long we need at least 15 bits
    // ready in the bit buffer to start decoding the next huffman code.
    if l.num_bits < 15 {
        // First, make sure there is enough data in the bit buffer to decode a huffman code.
        if in_iter.len() < 2 {
            // If there is less than 2 bytes left in the input buffer, we try to look up
            // the huffman code with what's available, and return if that doesn't succeed.
            // Original explanation in miniz:
            // /* TINFL_HUFF_BITBUF_FILL() is only used rarely, when the number of bytes
            //  * remaining in the input buffer falls below 2. */
            // /* It reads just enough bytes from the input stream that are needed to decode
            //  * the next Huffman code (and absolutely no more). It works by trying to fully
            //  * decode a */
            // /* Huffman code by using whatever bits are currently present in the bit buffer.
            //  * If this fails, it reads another byte, and tries again until it succeeds or
            //  * until the */
            // /* bit buffer contains >=15 bits (deflate's max. Huffman code size). */
            loop {
                let mut temp = r.tables[table].fast_lookup(l.bit_buf) as i32;

                if temp >= 0 {
                    let code_len = (temp >> 9) as u32;
                    if (code_len != 0) && (l.num_bits >= code_len) {
                        break;
                    }
                } else if l.num_bits > FAST_LOOKUP_BITS.into() {
                    let mut code_len = FAST_LOOKUP_BITS as u32;
                    loop {
                        temp =
                            r.tables[table].tree[(!temp + ((l.bit_buf >> code_len) & 1) as i32) as
                                                     usize] as i32;
                        code_len += 1;
                        if temp >= 0 || l.num_bits < code_len + 1 {
                            break;
                        }
                    }
                    if temp >= 0 {
                        break;
                    }
                }

                // TODO: miniz jumps straight to here after getting here again after failing to read
                // a byte.
                // Doing that lets miniz avoid re-doing the lookup that that was done in the
                // previous call.
                let mut byte = 0;
                if let a @ Action::End(_) =
                    read_byte(in_iter, flags, |b| {
                        byte = b;
                        Action::None
                    })
                {
                    return a;
                };

                // Do this outside closure for now to avoid borrowing r.
                l.bit_buf |= (byte as BitBuffer) << l.num_bits;
                l.num_bits += 8;

                if l.num_bits >= 15 {
                    break;
                }
            }
        } else {
            // There is enough data in the input buffer, so read the next two bytes
            // and add them to the bit buffer.
            // Unwrapping here is fine since we just checked that there are at least two
            // bytes left.
            let b0 = *in_iter.next().unwrap() as BitBuffer;
            let b1 = *in_iter.next().unwrap() as BitBuffer;

            l.bit_buf |= (b0 << l.num_bits) | (b1 << l.num_bits + 8);
            l.num_bits += 16;
        }
    }

    // We now have at least 15 bits in the input buffer.
    let mut symbol = r.tables[table].fast_lookup(l.bit_buf) as i32;
    let code_len;
    // If the symbol was found in the fast lookup table.
    if symbol >= 0 {
        // Get the length value from the top bits.
        // As we shift down the sign bit, converting to an unsigned value
        // shouldn't overflow.
        code_len = (symbol >> 9) as u32;
        // Mask out the length value.
        symbol &= 511;
    } else {
        let res = r.tables[table].tree_lookup(symbol, l.bit_buf, FAST_LOOKUP_BITS as u32);
        symbol = res.0;
        code_len = res.1 as u32;
    };

    l.bit_buf >>= code_len as u32;
    l.num_bits -= code_len;
    f(r, l, symbol)
}

#[inline]
fn read_byte<F>(in_iter: &mut slice::Iter<u8>, flags: u32, f: F) -> Action
where
    F: FnOnce(u8) -> Action,
{
    match in_iter.next() {
        None => end_of_input(flags),
        Some(&byte) => f(byte),
    }
}

#[inline]
fn read_bits<F>(
    l: &mut LocalVars,
    amount: u32,
    in_iter: &mut slice::Iter<u8>,
    flags: u32,
    f: F,
) -> Action
where
    F: FnOnce(&mut LocalVars, BitBuffer) -> Action,
{
    while l.num_bits < amount {
        match read_byte(in_iter, flags, |byte| {
            l.bit_buf |= (byte as BitBuffer) << l.num_bits;
            l.num_bits += 8;
            Action::None
        }) {
            Action::None => (),
            action => return action,
        }
    }

    let bits = l.bit_buf & ((1 << amount) - 1);
    l.bit_buf >>= amount;
    l.num_bits -= amount;
    f(l, bits)
}

#[inline]
fn pad_to_bytes<F>(l: &mut LocalVars, in_iter: &mut slice::Iter<u8>, flags: u32, f: F) -> Action
where
    F: FnOnce(&mut LocalVars) -> Action,
{
    let num_bits = l.num_bits & 7;
    read_bits(l, num_bits, in_iter, flags, |l, _| f(l))
}

#[inline]
fn end_of_input(flags: u32) -> Action {
    Action::End(if flags & TINFL_FLAG_HAS_MORE_INPUT != 0 {
        TINFLStatus::NeedsMoreInput
    } else {
        TINFLStatus::FailedCannotMakeProgress
    })
}

#[inline]
fn undo_bytes(l: &mut LocalVars, max: u32) -> u32 {
    let res = cmp::min((l.num_bits >> 3), max);
    l.num_bits -= res << 3;
    res
}

fn start_static_table(r: &mut DecompressorOxide) {
    r.table_sizes[LITLEN_TABLE] = 288;
    r.table_sizes[DIST_TABLE] = 32;
    memset(&mut r.tables[LITLEN_TABLE].code_size[0..144], 8);
    memset(&mut r.tables[LITLEN_TABLE].code_size[144..256], 9);
    memset(&mut r.tables[LITLEN_TABLE].code_size[256..280], 7);
    memset(&mut r.tables[LITLEN_TABLE].code_size[280..288], 8);
    memset(&mut r.tables[DIST_TABLE].code_size[0..32], 5);
}

fn init_tree(r: &mut DecompressorOxide, l: &mut LocalVars) -> Action {
    loop {
        let table = &mut r.tables[r.block_type as usize];
        let table_size = r.table_sizes[r.block_type as usize] as usize;
        let mut total_symbols = [0u32; 16];
        let mut next_code = [0u32; 17];
        memset(&mut table.look_up[..], 0);
        memset(&mut table.tree[..], 0);

        for &code_size in &table.code_size[..table_size] {
            total_symbols[code_size as usize] += 1;
        }
        let mut used_symbols = 0;
        let mut total = 0;
        for i in 1..16 {
            used_symbols += total_symbols[i];
            total += total_symbols[i];
            total <<= 1;
            next_code[i + 1] = total;
        }

        if total != 65_536 && used_symbols > 1 {
            return Action::Jump(BadTotalSymbols);
        }

        let mut tree_next = -1;
        for symbol_index in 0..table_size {
            let mut rev_code = 0;
            let code_size = table.code_size[symbol_index];
            if code_size == 0 {
                continue;
            }

            let mut cur_code = next_code[code_size as usize];
            next_code[code_size as usize] += 1;

            for _ in 0..code_size {
                rev_code = (rev_code << 1) | (cur_code & 1);
                cur_code >>= 1;
            }

            if code_size <= FAST_LOOKUP_BITS {
                let k = ((code_size as i16) << 9) | symbol_index as i16;
                while rev_code < FAST_LOOKUP_SIZE {
                    table.look_up[rev_code as usize] = k;
                    rev_code += 1 << code_size;
                }
                continue;
            }

            let mut tree_cur = table.look_up[(rev_code & (FAST_LOOKUP_SIZE - 1)) as usize];
            if tree_cur == 0 {
                table.look_up[(rev_code & (FAST_LOOKUP_SIZE - 1)) as usize] = tree_next as
                    i16;
                tree_cur = tree_next;
                tree_next -= 2;
            }

            rev_code >>= FAST_LOOKUP_BITS - 1;
            for _ in FAST_LOOKUP_BITS + 1..code_size {
                rev_code >>= 1;
                tree_cur -= (rev_code & 1) as i16;
                if table.tree[(-tree_cur - 1) as usize] == 0 {
                    table.tree[(-tree_cur - 1) as usize] = tree_next as i16;
                    tree_cur = tree_next;
                    tree_next -= 2;
                } else {
                    tree_cur = table.tree[(-tree_cur - 1) as usize];
                }
            }

            rev_code >>= 1;
            tree_cur -= (rev_code & 1) as i16;
            table.tree[(-tree_cur - 1) as usize] = symbol_index as i16;
        }

        if r.block_type == 2 {
            l.counter = 0;
            return Action::Jump(ReadLitlenDistTablesCodeSize);
        }

        if r.block_type == 0 {
            break;
        }
        r.block_type -= 1;
    }

    l.counter = 0;
    Action::Jump(DecodeLitlen)
}

// A helper macro for generating the state machine.
//
// As Rust doesn't have fallthrough on matches, we have to return to the match statement
// and jump for each state change. (Which would ideally be optimized away, but often isn't.)
macro_rules! generate_state {
    ($state: ident, $state_machine: tt, $f: expr) => {
        loop {
            match $f {
                Action::None => continue,
                Action::Jump(new_state) => {
                    $state = new_state;
                    continue $state_machine;
                },
                Action::End(result) => break $state_machine result,
            }
        }
    };
}

#[derive(Copy, Clone)]
struct LocalVars {
    pub bit_buf: BitBuffer,
    pub num_bits: u32,
    pub dist: u32,
    pub counter: u32,
    pub num_extra: u32,
    pub dist_from_out_buf_start: usize,
}

/// Wrapper for read_bits/bytes that jumps out if reading failed.
/// This could possibly be done cleaner with ? later.
macro_rules! try_read {
    ($f: expr) => {
        if let Action::End(s) = $f {
            break s;
        }
    }
}

/// Fast inner decompression loop which is run  while there is at least
/// 259 bytes left in the output buffer, and at least 6 bytes left in the input buffer
/// (The maximum one match would need + 1).
///
/// This was inspired by a similar optimization in zlib, which uses this info to do
/// faster unchecked copies of multiple bytes at a time.
/// Currently we don't do this here, but this function does avoid having to jump through the
/// big match loop on each state change(as rust does not have fallthrough or gotos at the moment),
/// and already improves decompression speed a fair bit.
fn decompress_fast(
    r: &mut DecompressorOxide,
    mut in_iter: &mut slice::Iter<u8>,
    out_buf: &mut OutputBuffer,
    flags: u32,
    local_vars: &mut LocalVars,
    out_buf_size_mask: usize,
) -> (TINFLStatus, State) {
    // Make a local copy of the most used variables, to avoid having to update and read from values
    // in a random memory location and to encourage more register use.
    let mut l = *local_vars;
    let mut state;

    let status: TINFLStatus = 'o: loop {
        'litlen: loop {
            state = State::DecodeLitlen;
            // This function assumes that there is at least 259 bytes left in the output buffer,
            // and that there is at least 6 bytes left in the input buffer.
            // We need the one extra byte as we may write one length and one full match
            // before checking again.
            if out_buf.bytes_left() < 259 || in_iter.len() < 6 {
                break 'o TINFLStatus::Done;
            }

            fill_bit_buffer(&mut l, &mut in_iter);

            let (symbol, code_len) = r.tables[LITLEN_TABLE].lookup(l.bit_buf);

            l.counter = symbol as u32;
            l.bit_buf >>= code_len;
            l.num_bits -= code_len;

            if (l.counter & 256) != 0 {
                // The symbol is not a literal.
                break;
            } else {
                // If we have a 32-bit buffer we need to read another two bytes now
                // to have enough bits to keep going.
                if cfg!(not(target_pointer_width = "64")) {
                    fill_bit_buffer(&mut l, &mut in_iter);
                }

                let (symbol, code_len) = r.tables[LITLEN_TABLE].lookup(l.bit_buf);

                l.bit_buf >>= code_len;
                l.num_bits -= code_len;
                // The previous symbol was a literal, so write it directly and check
                // the next one.
                out_buf.write_byte(l.counter as u8);
                if (symbol & 256) != 0 {
                    l.counter = symbol as u32;
                    // The symbol is a length value.
                    break;
                } else {
                    // The symbol is a literal, so write it directly and continue.
                    out_buf.write_byte(symbol as u8);
                }
            }
        }

        state.begin(HuffDecodeOuterLoop1);
        // Mask the top bits since they may contain length info.
        l.counter &= 511;

        if l.counter == 256 {
            // We hit the end of block symbol.
            state.begin(BlockDone);
            break 'o TINFLStatus::Done;
        } else {
            // The symbol was a length code.
            // # Optimization
            // Mask the value to avoid bounds checks
            // We could use get_unchecked later if can statically verify that
            // this will never go out of bounds.
            l.num_extra = LENGTH_EXTRA[(l.counter - 257) as usize & BASE_EXTRA_MASK] as u32;
            l.counter = LENGTH_BASE[(l.counter - 257) as usize & BASE_EXTRA_MASK] as u32;
            // Length and distance codes have a number of extra bits depending on
            // the base, which together with the base gives us the exact value.
            if l.num_extra != 0 {
                state.begin(ReadExtraBitsLitlen);
                let num_extra = l.num_extra;
                try_read!(read_bits(
                    &mut l,
                    num_extra,
                    &mut in_iter,
                    flags,
                    |l, extra_bits| {
                        l.counter += extra_bits as u32;
                        Action::None
                    },
                ));
            }

            // We found a length code, so a distance code should follow.
            state.begin(DecodeDistance);
            match decode_huffman_code(
                r,
                &mut l,
                DIST_TABLE,
                flags,
                &mut in_iter,
                |_r, l, symbol| {
                    // # Optimization
                    // Mask the value to avoid bounds checks
                    // We could use get_unchecked later if can statically verify that
                    // this will never go out of bounds.
                    l.num_extra = DIST_EXTRA[symbol as usize & BASE_EXTRA_MASK] as u32;
                    l.dist = DIST_BASE[symbol as usize & BASE_EXTRA_MASK] as u32;
                    if l.num_extra != 0 {
                        // ReadEXTRA_BITS_DISTACNE
                        Action::Jump(ReadExtraBitsDistance)
                    } else {
                        Action::Jump(HuffDecodeOuterLoop2)
                    }
                },
            ) {
                // We decoded a distance code, read the extra bits if needed to find the
                // full distance value.
                Action::Jump(to) if to == ReadExtraBitsDistance => {
                    state.begin(ReadExtraBitsDistance);
                    let num_extra = l.num_extra;
                    try_read!(read_bits(
                        &mut l,
                        num_extra,
                        &mut in_iter,
                        flags,
                        |l, extra_bits| {
                            l.dist += extra_bits as u32;
                            Action::None
                        },
                    ));
                }
                // There were no extra bits.
                Action::End(s) => break s,
                _ => (),
            };

            l.dist_from_out_buf_start = out_buf.position();
            if l.dist as usize > l.dist_from_out_buf_start &&
                (flags & TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF != 0)
            {
                // We encountered a distance that refers a position before
                // the start of the decoded data, so we can't continue.
                state.begin(DistanceOutOfBounds);
                break TINFLStatus::Failed;
            }

            let mut source_pos = (l.dist_from_out_buf_start.wrapping_sub(l.dist as usize)) &
                out_buf_size_mask;

            let mut out_pos = l.dist_from_out_buf_start;

            // There isn't enough space left for all of the match in the (wrapping) out buffer,
            // so break out.
            if source_pos >= out_pos && (source_pos - out_pos) < l.counter as usize {
                // Jump to the slow decode loop.
                // Alternatively, we could replicate the code for this condition from there, and
                // jump directly to WriteLenBytestoend, which may or may not be more efficient,
                // though would add more code duplication.
                state.begin(HuffDecodeOuterLoop2);
                break TINFLStatus::Done;
            }

            // The match is ok, and there is space so output the decoded match data.
            {
                let out_slice = out_buf.get_mut();
                let mut match_len = l.counter as usize;
                // This is faster on x86, but at least on arm, it's slower, so only
                // use it on x86 for now.
                // TODO: We know that there will always be at least 258 bytes of space
                // in the output buffer, so maybe we can use that to do big copies without
                // checking.
                if match_len <= l.dist as usize && match_len > 3 &&
                    source_pos + match_len < out_slice.len() &&
                    (cfg!(any(target_arch = "x86", target_arch = "x86_64")))
                {
                    if source_pos < out_pos {
                        let (from_slice, to_slice) = out_slice.split_at_mut(out_pos);
                        to_slice[..match_len].copy_from_slice(
                            &from_slice[source_pos..
                                            source_pos + match_len],
                        )
                    } else {
                        let (to_slice, from_slice) = out_slice.split_at_mut(source_pos);
                        to_slice[out_pos..out_pos + match_len].copy_from_slice(
                            &from_slice
                                [..match_len],
                        );
                    }
                    out_pos += match_len;
                } else {
                    // We are not on x86, there was an overlapping match,
                    // or the match is wrapping, so copy manually.
                    // The loop exit condition is at the end since a match
                    // will always be at least 3 bytes long.
                    loop {
                        // We mask the source position so we get the correct bytes
                        // if the match wraps around.
                        out_slice[out_pos] =
                            out_slice[source_pos & out_buf_size_mask];
                        out_slice[out_pos + 1] =
                            out_slice[(source_pos + 1) & out_buf_size_mask];
                        out_slice[out_pos + 2] =
                            out_slice[(source_pos + 2) & out_buf_size_mask];
                        source_pos += 3;
                        out_pos += 3;
                        match_len -= 3;
                        if match_len < 3 {
                            break;
                        }
                    }

                    if match_len > 0 {
                        out_slice[out_pos] =
                            out_slice[source_pos & out_buf_size_mask];
                        if match_len > 1 {
                            out_slice[out_pos + 1] =
                                out_slice[(source_pos + 1) & out_buf_size_mask];
                        }
                        out_pos += match_len;
                    }
                    l.counter = match_len as u32;
                }
            }
            out_buf.set_position(out_pos);
        }
    };

    *local_vars = l;
    (status, state)
}

/// Main decompression function. Keeps decompressing data from `in_buf` until the in_buf is emtpy,
/// out_cur is full, the end of the deflate stream is hit, or there is an error in the deflate
/// stream.
///
/// The position of the output cursor indicates where in the output buffer slice writing should
/// start.
///
/// # Returns
/// returns a tuple containing the status of the compressor, the number of input bytes read, and the
/// number of bytes output to `out_cur`.
/// Updates `out_cur` with new position.
///
/// This function shouldn't panic pending any bugs.
pub fn decompress(
    r: &mut DecompressorOxide,
    in_buf: &[u8],
    out_cur: &mut Cursor<&mut [u8]>,
    flags: u32,
) -> (TINFLStatus, usize, usize) {
    let res = decompress_inner(r, in_buf, out_cur, flags);
    let new_pos = out_cur.position() + res.2 as u64;
    out_cur.set_position(new_pos);
    res
}

#[inline]
fn decompress_inner(
    r: &mut DecompressorOxide,
    in_buf: &[u8],
    out_cur: &mut Cursor<&mut [u8]>,
    flags: u32,
) -> (TINFLStatus, usize, usize) {
    let out_buf_start_pos = out_cur.position() as usize;
    let out_buf_size_mask = if flags & TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF != 0 {
        usize::max_value()
    } else {
        out_cur.get_ref().len() - 1
    };

    // Ensure the output buffer's size is a power of 2, unless the output buffer
    // is large enough to hold the entire output file (in which case it doesn't
    // matter).
    // Also make sure that the output buffer position is not past the end of the output buffer.
    if (out_buf_size_mask.wrapping_add(1) & out_buf_size_mask) != 0  ||
        out_cur.position() > out_cur.get_ref().len() as u64 {
        return (TINFLStatus::BadParam, 0, 0);
    }

    let mut in_iter = in_buf.iter();

    let mut state = r.state;

    let mut out_buf = OutputBuffer::from_slice_and_pos(out_cur.get_mut(), out_buf_start_pos);

    // TODO: Borrow instead of Copy
    let mut l = LocalVars {
        bit_buf: r.bit_buf,
        num_bits: r.num_bits,
        dist: r.dist,
        counter: r.counter,
        num_extra: r.num_extra,
        dist_from_out_buf_start: r.dist_from_out_buf_start,
    };

    let mut status = 'state_machine: loop {
        match state {
            Start => {
                generate_state!(state, 'state_machine, {
                l.bit_buf = 0;
                l.num_bits = 0;
                l.dist = 0;
                l.counter = 0;
                l.num_extra = 0;
                r.z_header0 = 0;
                r.z_header1 = 0;
                r.z_adler32 = 1;
                r.check_adler32 = 1;
                if flags & TINFL_FLAG_PARSE_ZLIB_HEADER != 0 {
                    Action::Jump(State::ReadZlibCmf)
                } else {
                    Action::Jump(State::ReadBlockHeader)
                }
            })
            }

            ReadZlibCmf => {
                generate_state!(state, 'state_machine, {
                read_byte(&mut in_iter, flags, |cmf| {
                    r.z_header0 = cmf as u32;
                    Action::Jump(State::ReadZlibFlg)
                })
            })
            }

            ReadZlibFlg => {
                generate_state!(state, 'state_machine, {
                read_byte(&mut in_iter, flags, |flg| {
                    r.z_header1 = flg as u32;
                    validate_zlib_header(r.z_header0, r.z_header1, flags, out_buf_size_mask)
                })
            })
            }

            // Read the block header and jump to the relevant section depending on the block type.
            ReadBlockHeader => {
                generate_state!(state, 'state_machine, {
                read_bits(&mut l, 3, &mut in_iter, flags, |l, bits| {
                    r.finish = (bits & 1) as u32;
                    r.block_type = (bits >> 1) as u32 & 3;
                    match r.block_type {
                        0 => Action::Jump(BlockTypeNoCompression),
                        1 => {
                            start_static_table(r);
                            init_tree(r, l)
                        },
                        2 => {
                            l.counter = 0;
                            Action::Jump(ReadTableSizes)
                        },
                        3 => Action::Jump(BlockTypeUnexpected),
                        _ => unreachable!()
                    }
                })
            })
            }

            // Raw/Stored/uncompressed block.
            BlockTypeNoCompression => {
                generate_state!(state, 'state_machine, {
                pad_to_bytes(&mut l, &mut in_iter, flags, |l| {
                    l.counter = 0;
                    Action::Jump(RawHeader)
                })
            })
            }

            // Check that the raw block header is correct.
            RawHeader => {
                generate_state!(state, 'state_machine, {
                if l.counter < 4 {
                    // Read block length and block length check.
                    if l.num_bits != 0 {
                        read_bits(&mut l, 8, &mut in_iter, flags, |l, bits| {
                            r.raw_header[l.counter as usize] = bits as u8;
                            l.counter += 1;
                            Action::None
                        })
                    } else {
                        read_byte(&mut in_iter, flags, |byte| {
                            r.raw_header[l.counter as usize] = byte;
                            l.counter += 1;
                            Action::None
                        })
                    }
                } else {
                    // Check if the length value of a raw block is correct.
                    // The 2 first (2-byte) words in a raw header are the length and the
                    // ones complement of the length.
                    let length = r.raw_header[0] as u16 | ((r.raw_header[1] as u16) << 8);
                    let check = (r.raw_header[2] as u16) | ((r.raw_header[3] as u16) << 8);
                    let valid = length == !check;
                    l.counter = length.into();

                    if !valid {
                        Action::Jump(BadRawLength)
                    } else if l.counter == 0 {
                        // Empty raw block. Sometimes used for syncronization.
                        Action::Jump(BlockDone)
                    } else if l.num_bits != 0 {
                        // There is some data in the bit buffer, so we need to write that first.
                        Action::Jump(RawReadFirstByte)
                    } else {
                        // The bit buffer is empty, so memcpy the rest of the uncompressed data from
                        // the block.
                        Action::Jump(RawMemcpy1)
                    }
                }
            })
            }

            // Read the byte from the bit buffer.
            RawReadFirstByte => {
                generate_state!(state, 'state_machine, {
                read_bits(&mut l, 8, &mut in_iter, flags, |l, bits| {
                    l.dist = bits as u32;
                    Action::Jump(RawStoreFirstByte)
                })
            })
            }

            // Write the byte we just read to the output buffer.
            RawStoreFirstByte => {
                generate_state!(state, 'state_machine, {
                if out_buf.bytes_left() == 0 {
                    Action::End(TINFLStatus::HasMoreOutput)
                } else {
                    out_buf.write_byte(l.dist as u8);
                    l.counter -= 1;
                    if l.counter == 0 || l.num_bits == 0 {
                        Action::Jump(RawMemcpy1)
                    } else {
                        // There is still some data left in the bit buffer that needs to be output.
                        // TODO: Changed this to jump to `RawReadfirstbyte` rather than
                        // `RawStoreFirstByte` as that seemed to be the correct path, but this
                        // needs testing.
                        Action::Jump(RawReadFirstByte)
                    }
                }
            })
            }

            RawMemcpy1 => {
                generate_state!(state, 'state_machine, {
                if l.counter == 0 {
                    Action::Jump(BlockDone)
                } else if out_buf.bytes_left() == 0 {
                    Action::End(TINFLStatus::HasMoreOutput)
                } else {
                    Action::Jump(RawMemcpy2)
                }
            })
            }

            RawMemcpy2 => {
                generate_state!(state, 'state_machine, {
                if in_iter.len() > 0 {
                    // Copy as many raw bytes as possible from the input to the output using memcpy.
                    // Raw block lengths are limited to 64 * 1024, so casting through usize and u32
                    // is not an issue.
                    let space_left = out_buf.bytes_left();
                    let bytes_to_copy = cmp::min(cmp::min(
                        space_left,
                        in_iter.len()),
                        l.counter as usize
                    );

                    out_buf.write_slice(&in_iter.as_slice()[..bytes_to_copy]);

                    (&mut in_iter).nth(bytes_to_copy - 1);
                    l.counter -= bytes_to_copy as u32;
                    Action::Jump(RawMemcpy1)
                } else {
                    end_of_input(flags)
                }
            })
            }

            // Read how many huffman codes/symbols are used for each table.
            ReadTableSizes => {
                generate_state!(state, 'state_machine, {
                if l.counter < 3 {
                    let num_bits = [5, 5, 4][l.counter as usize];
                    read_bits(&mut l, num_bits, &mut in_iter, flags, |l, bits| {
                        r.table_sizes[l.counter as usize] =
                            bits as u32 + MIN_TABLE_SIZES[l.counter as usize] as u32;
                        l.counter += 1;
                        Action::None
                    })
                } else {
                    memset(&mut r.tables[HUFFLEN_TABLE].code_size[..], 0);
                    l.counter = 0;
                    Action::Jump(ReadHufflenTableCodeSize)
                }
            })
            }

            // Read the 3-bit lengths of the huffman codes describing the huffman code lengths used
            // to decode the lengths of the main tables.
            ReadHufflenTableCodeSize => {
                generate_state!(state, 'state_machine, {
                if l.counter < r.table_sizes[HUFFLEN_TABLE] {
                    read_bits(&mut l, 3, &mut in_iter, flags, |l, bits| {
                        // These lengths are not stored in a normal ascending order, but rather one
                        // specified by the deflate specification intended to put the most used
                        // values at the front as trailing zero lengths do not have to be stored.
                        r.tables[HUFFLEN_TABLE]
                            .code_size[HUFFMAN_LENGTH_ORDER[l.counter as usize] as usize]
                            = bits as u8;
                        l.counter += 1;
                        Action::None
                    })
                } else {
                    r.table_sizes[HUFFLEN_TABLE] = 19;
                    init_tree(r, &mut l)
                }
            })
            }

            ReadLitlenDistTablesCodeSize => {
                generate_state!(state, 'state_machine, {
                if l.counter < r.table_sizes[LITLEN_TABLE] + r.table_sizes[DIST_TABLE] {
                    decode_huffman_code(
                        r, &mut l, HUFFLEN_TABLE,
                        flags, &mut in_iter, |r, l, symbol| {
                        l.dist = symbol as u32;
                        if l.dist < 16 {
                            r.len_codes[l.counter as usize] = l.dist as u8;
                            l.counter += 1;
                            Action::None
                        } else if l.dist == 16 && l.counter == 0 {
                            Action::Jump(BadCodeSizeDistPrevLookup)
                        } else {
                            l.num_extra = [2, 3, 7][l.dist as usize - 16];
                            Action::Jump(ReadExtraBitsCodeSize)
                        }
                    })
                } else if l.counter != r.table_sizes[LITLEN_TABLE] + r.table_sizes[DIST_TABLE] {
                    Action::Jump(BadCodeSizeSum)
                } else {
                    r.tables[LITLEN_TABLE].code_size[..r.table_sizes[LITLEN_TABLE] as usize]
                        .copy_from_slice(&r.len_codes[..r.table_sizes[LITLEN_TABLE] as usize]);

                    let dist_table_start = r.table_sizes[LITLEN_TABLE] as usize;
                    let dist_table_end = (r.table_sizes[LITLEN_TABLE] +
                                          r.table_sizes[DIST_TABLE]) as usize;
                    r.tables[DIST_TABLE].code_size[..r.table_sizes[DIST_TABLE] as usize]
                        .copy_from_slice(&r.len_codes[dist_table_start..dist_table_end]);

                    r.block_type -= 1;
                    init_tree(r, &mut l)
                }
            })
            }

            ReadExtraBitsCodeSize => {
                generate_state!(state, 'state_machine, {
                let num_extra = l.num_extra;
                read_bits(&mut l, num_extra, &mut in_iter, flags, |l, mut extra_bits| {
                    // Mask to avoid a bounds check.
                    extra_bits += [3, 3, 11][(l.dist as usize - 16) & 3];
                    let val = if l.dist == 16 {
                        r.len_codes[l.counter as usize - 1]
                    } else {
                        0
                    };

                    memset(
                        &mut r.len_codes[
                            l.counter as usize..l.counter as usize + extra_bits as usize],
                        val);
                    l.counter += extra_bits as u32;
                    Action::Jump(ReadLitlenDistTablesCodeSize)
                })
            })
            }

            DecodeLitlen => {
                generate_state!(state, 'state_machine, {
                if in_iter.len() < 4 || out_buf.bytes_left() < 2 {
                    // See if we can decode a literal with the data we have left.
                    // Jumps to next state (WriteSymbol) if successful.
                    decode_huffman_code(
                        r, &mut l, LITLEN_TABLE, flags, &mut in_iter, |_r, l, symbol| {
                        l.counter = symbol as u32;
                        Action::Jump(WriteSymbol)
                        })
                } else if
                // If there is enough space, use the fast inner decompression
                // function.
                    out_buf.bytes_left() >= 259 &&
                    in_iter.len() >= 6
                {
                    let (status, new_state) =
                        decompress_fast(r,
                                        &mut in_iter,
                                        &mut out_buf,
                                        flags,
                                        &mut l,
                                        out_buf_size_mask);
                    state = new_state;
                    if status == TINFLStatus::Done {
                        Action::Jump(new_state)
                    } else {
                        Action::End(status)
                    }
                } else {
                    fill_bit_buffer(&mut l, &mut in_iter);

                    let (symbol, code_len) = r.tables[LITLEN_TABLE].lookup(l.bit_buf);

                    l.counter = symbol as u32;
                    l.bit_buf >>= code_len;
                    l.num_bits -= code_len;

                    if (l.counter & 256) != 0 {
                        // The symbol is not a literal.
                        Action::Jump(HuffDecodeOuterLoop1)
                    } else {
                        // If we have a 32-bit buffer we need to read another two bytes now
                        // to have enough bits to keep going.
                        if cfg!(not(target_pointer_width = "64")) {
                            fill_bit_buffer(&mut l, &mut in_iter);
                        }

                        let (symbol, code_len) = r.tables[LITLEN_TABLE].lookup(l.bit_buf);

                        l.bit_buf >>= code_len;
                        l.num_bits -= code_len;
                        // The previous symbol was a literal, so write it directly and check
                        // the next one.
                        out_buf.write_byte(l.counter as u8);
                        if (symbol & 256) != 0 {
                            l.counter = symbol as u32;
                            // The symbol is a length value.
                            Action::Jump(HuffDecodeOuterLoop1)
                        } else {
                            // The symbol is a literal, so write it directly and continue.
                            out_buf.write_byte(symbol as u8);
                            Action::None
                        }
                    }
                }
            })
            }

            WriteSymbol => {
                generate_state!(state, 'state_machine, {
                if l.counter >= 256 {
                    Action::Jump(HuffDecodeOuterLoop1)
                } else if out_buf.bytes_left() > 0 {
                    out_buf.write_byte(l.counter as u8);
                    Action::Jump(DecodeLitlen)
                } else {
                    Action::End(TINFLStatus::HasMoreOutput)
                }
            })
            }

            HuffDecodeOuterLoop1 => {
                generate_state!(state, 'state_machine, {
                // Mask the top bits since they may contain length info.
                l.counter &= 511;

                if l.counter == 256 {
                    // We hit the end of block symbol.
                    Action::Jump(BlockDone)
                } else {
                    // # Optimization
                    // Mask the value to avoid bounds checks
                    // We could use get_unchecked later if can statically verify that
                    // this will never go out of bounds.
                    l.num_extra = LENGTH_EXTRA[(l.counter - 257) as usize & BASE_EXTRA_MASK] as u32;
                    l.counter = LENGTH_BASE[(l.counter - 257) as usize & BASE_EXTRA_MASK] as u32;
                    // Length and distance codes have a number of extra bits depending on
                    // the base, which together with the base gives us the exact value.
                    if l.num_extra != 0 {
                        Action::Jump(ReadExtraBitsLitlen)
                    } else {
                        Action::Jump(DecodeDistance)
                    }
                }
            })
            }

            ReadExtraBitsLitlen => {
                generate_state!(state, 'state_machine, {
                let num_extra = l.num_extra;
                read_bits(&mut l, num_extra, &mut in_iter, flags, |l, extra_bits| {
                    l.counter += extra_bits as u32;
                    Action::Jump(DecodeDistance)
                })
            })
            }

            DecodeDistance => {
                generate_state!(state, 'state_machine, {
                // Try to read a huffman code from the input buffer and look up what
                // length code the decoded symbol refers to.
                decode_huffman_code(r, &mut l, DIST_TABLE, flags, &mut in_iter, |_r, l, symbol| {
                    // # Optimization
                    // Mask the value to avoid bounds checks
                    // We could use get_unchecked later if can statically verify that
                    // this will never go out of bounds.
                    l.num_extra = DIST_EXTRA[symbol as usize & BASE_EXTRA_MASK] as u32;
                    l.dist = DIST_BASE[symbol as usize & BASE_EXTRA_MASK] as u32;
                    if l.num_extra != 0 {
                        // ReadEXTRA_BITS_DISTACNE
                        Action::Jump(ReadExtraBitsDistance)
                    } else {
                        Action::Jump(HuffDecodeOuterLoop2)
                    }
                })
            })
            }

            ReadExtraBitsDistance => {
                generate_state!(state, 'state_machine, {
                let num_extra = l.num_extra;
                read_bits(&mut l, num_extra, &mut in_iter, flags, |l, extra_bits| {
                    l.dist += extra_bits as u32;
                    Action::Jump(HuffDecodeOuterLoop2)
                })
            })
            }

            HuffDecodeOuterLoop2 => {
                generate_state!(state, 'state_machine, {
                l.dist_from_out_buf_start = out_buf.position();
                if l.dist as usize > l.dist_from_out_buf_start &&
                    (flags & TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF != 0) {
                        // We encountered a distance that refers a position before
                        // the start of the decoded data, so we can't continue.
                        Action::Jump(DistanceOutOfBounds)
                } else {
                    let mut source_pos = (l.dist_from_out_buf_start.wrapping_sub(l.dist as usize)) &
                        out_buf_size_mask;

                    let out_len = out_buf.get_ref().len() as usize;
                    let match_end_pos = cmp::max(source_pos, out_buf.position())
                        + l.counter as usize;

                    let mut out_pos = out_buf.position();

                    if match_end_pos > out_len ||
                        // miniz doesn't do this check here. Not sure how it makes sure
                        // that this case doesn't happen.
                        (source_pos >= out_pos && (source_pos - out_pos) < l.counter as usize) {
                        // Not enough space for all of the data in the output buffer,
                        // so copy what we have space for.
                        if l.counter == 0 {
                            Action::Jump(DecodeLitlen)
                        } else {
                            l.counter -= 1;
                            Action::Jump(WriteLenBytesToEnd)
                        }
                    } else {
                        {
                            let out_slice = out_buf.get_mut();
                            let mut match_len = l.counter as usize;
                            // This is faster on x86, but at least on arm, it's slower, so only
                            // use it on x86 for now.
                            if match_len <= l.dist as usize && match_len > 3 &&
                                (cfg!(any(target_arch = "x86", target_arch ="x86_64"))){
                                if source_pos < out_pos {
                                    let (from_slice, to_slice) = out_slice.split_at_mut(out_pos);
                                    to_slice[..match_len]
                                        .copy_from_slice(
                                            &from_slice[source_pos..source_pos + match_len])
                                } else {
                                    let (to_slice, from_slice) = out_slice.split_at_mut(source_pos);
                                    to_slice[out_pos..out_pos + match_len]
                                        .copy_from_slice(&from_slice[..match_len]);
                                }
                                out_pos += match_len;
                            } else {
                                loop {
                                    out_slice[out_pos] = out_slice[source_pos];
                                    out_slice[out_pos + 1] = out_slice[source_pos + 1];
                                    out_slice[out_pos + 2] = out_slice[source_pos + 2];
                                    source_pos += 3;
                                    out_pos += 3;
                                    match_len -= 3;
                                    if match_len < 3 {
                                        break;
                                    }
                                }

                                if match_len > 0 {
                                    out_slice[out_pos] = out_slice[source_pos];
                                    if match_len > 1 {
                                        out_slice[out_pos + 1] = out_slice[source_pos + 1];
                                    }
                                    out_pos += match_len;
                                }
                                l.counter = match_len as u32;
                            }
                        }

                        out_buf.set_position(out_pos);
                        Action::Jump(DecodeLitlen)
                    }
                }
            })
            }

            WriteLenBytesToEnd => {
                generate_state!(state, 'state_machine, {
                if out_buf.bytes_left() > 0 {
                    let source_pos =
                        l.dist_from_out_buf_start.wrapping_sub(l.dist as usize) & out_buf_size_mask;
                    let val = out_buf.get_ref()[source_pos];
                    l.dist_from_out_buf_start += 1;
                    out_buf.write_byte(val);
                    if l.counter == 0 {
                        Action::Jump(DecodeLitlen)
                    } else {
                        l.counter -= 1;
                        Action::None
                    }
                } else {
                    Action::End(TINFLStatus::HasMoreOutput)
                }
            })
            }

            BlockDone => {
                generate_state!(state, 'state_machine, {
                // End once we've read the last block.
                if r.finish != 0 {
                    pad_to_bytes(&mut l, &mut in_iter, flags, |_| Action::None);

                    let in_consumed = in_buf.len() - in_iter.len();
                    let undo = undo_bytes(&mut l, in_consumed as u32) as usize;
                    in_iter = in_buf[in_consumed - undo..].iter();

                    l.bit_buf &= (((1 as BitBuffer) << l.num_bits) - 1) as BitBuffer;
                    debug_assert_eq!(l.num_bits, 0);

                    if flags & TINFL_FLAG_PARSE_ZLIB_HEADER != 0 {
                        l.counter = 0;
                        Action::Jump(ReadAdler32)
                    } else {
                        Action::Jump(DoneForever)
                    }
                } else {
                    Action::Jump(ReadBlockHeader)
                }
            })
            }

            ReadAdler32 => {
                generate_state!(state, 'state_machine, {
                if l.counter < 4 {
                    if l.num_bits != 0 {
                        read_bits(&mut l, 8, &mut in_iter, flags, |l, bits| {
                            r.z_adler32 <<= 8;
                            r.z_adler32 |= bits as u32;
                            l.counter += 1;
                            Action::None
                        })
                    } else {
                        read_byte(&mut in_iter, flags, |byte| {
                            r.z_adler32 <<= 8;
                            r.z_adler32 |= byte as u32;
                            l.counter += 1;
                            Action::None
                        })
                    }
                } else {
                    Action::Jump(DoneForever)
                }
            })
            }

            // We are done.
            DoneForever => break TINFLStatus::Done,

            // Anything else indicates failure.
            // BadZlibHeader | BadRawLength | BlockTypeUnexpected | DistanceOutOfBounds |
            // BadTotalSymbols | BadCodeSizeDistPrevLookup | BadCodeSizeSum
            _ => break TINFLStatus::Failed,
        };
    };

    let in_undo = if status != TINFLStatus::NeedsMoreInput &&
        status != TINFLStatus::FailedCannotMakeProgress
    {
        undo_bytes(&mut l, (in_buf.len() - in_iter.len()) as u32) as usize
    } else {
        0
    };

    r.state = state.into();
    r.bit_buf = l.bit_buf;
    r.num_bits = l.num_bits;
    r.dist = l.dist;
    r.counter = l.counter;
    r.num_extra = l.num_extra;
    r.dist_from_out_buf_start = l.dist_from_out_buf_start;

    r.bit_buf &= (((1 as BitBuffer) << r.num_bits) - 1) as BitBuffer;

    // If this is a zlib stream, and update the adler32 checksum with the decompressed bytes if
    // requested.
    let need_adler = flags & (TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_COMPUTE_ADLER32) != 0;
    if need_adler && status as i32 >= 0 {
        let out_buf_pos = out_buf.position();
        r.check_adler32 = update_adler32(
            r.check_adler32,
            &out_buf.get_ref()[out_buf_start_pos..out_buf_pos],
        );

        // Once we are done, check if the checksum matches with the one provided in the zlib header.
        if status == TINFLStatus::Done && flags & TINFL_FLAG_PARSE_ZLIB_HEADER != 0 &&
            r.check_adler32 != r.z_adler32
        {
            status = TINFLStatus::Adler32Mismatch;
        }
    }

    // NOTE: Status here and in miniz_tester doesn't seem to match.
    (
        status,
        in_buf.len() - in_iter.len() - in_undo,
        out_buf.position() - out_buf_start_pos,
    )
}


#[cfg(test)]
mod test {
    use super::*;
    //use std::io::Cursor;

    //TODO: Fix these.

    fn tinfl_decompress_oxide<'i>(
        r: &mut DecompressorOxide,
        input_buffer: &'i [u8],
        output_buffer: &mut [u8],
        flags: u32,
    ) -> (TINFLStatus, &'i [u8], usize) {
        let (status, in_pos, out_pos) =
            decompress(r, input_buffer, &mut Cursor::new(output_buffer), flags);
        (status, &input_buffer[in_pos..], out_pos)
    }

    #[test]
    fn decompress_zlib() {
        let encoded = [
            120, 156, 243, 72, 205, 201, 201, 215, 81, 168,
            202, 201,  76,  82,  4,   0,  27, 101,  4,  19,
        ];
        let flags = TINFL_FLAG_COMPUTE_ADLER32 | TINFL_FLAG_PARSE_ZLIB_HEADER;

        let mut b = DecompressorOxide::new();
        const LEN: usize = 32;
        let mut b_buf = vec![0; LEN];

        // This should fail with the out buffer being to small.
        let b_status = tinfl_decompress_oxide(&mut b, &encoded[..], b_buf.as_mut_slice(), flags);

        assert_eq!(b_status.0, TINFLStatus::Failed);

        let flags = flags | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF;

        b = DecompressorOxide::new();

        // With TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF set this should no longer fail.
        let b_status = tinfl_decompress_oxide(&mut b, &encoded[..], b_buf.as_mut_slice(), flags);

        assert_eq!(b_buf[..b_status.2], b"Hello, zlib!"[..]);
        assert_eq!(b_status.0, TINFLStatus::Done);
    }

    #[test]
    fn raw_block() {
        const LEN: usize = 64;

        let text = b"Hello, zlib!";
        let encoded = {
            let len = text.len();
            let notlen = !len;
            let mut encoded =
                vec![1, len as u8, (len >> 8) as u8, notlen as u8, (notlen >> 8) as u8];
            encoded.extend_from_slice(&text[..]);
            encoded
        };

        //let flags = TINFL_FLAG_COMPUTE_ADLER32 | TINFL_FLAG_PARSE_ZLIB_HEADER |
        let flags = TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF;

        let mut b = DecompressorOxide::new();

        let mut b_buf = vec![0; LEN];

        let b_status = tinfl_decompress_oxide(&mut b, &encoded[..], b_buf.as_mut_slice(), flags);
        assert_eq!(b_buf[..b_status.2], text[..]);
        assert_eq!(b_status.0, TINFLStatus::Done);
    }

    fn masked_lookup(table: &HuffmanTable, bit_buf: BitBuffer) -> (i32, u32) {
        let ret = table.lookup(bit_buf);
        (ret.0 & 511, ret.1)
    }

    #[test]
    fn fixed_table_lookup() {
        let mut d = DecompressorOxide::new();
        d.block_type = 1;
        start_static_table(&mut d);
        let mut l = LocalVars {
            bit_buf: d.bit_buf,
            num_bits: d.num_bits,
            dist: d.dist,
            counter: d.counter,
            num_extra: d.num_extra,
            dist_from_out_buf_start: d.dist_from_out_buf_start,
        };
        init_tree(&mut d, &mut l);
        let llt = &d.tables[LITLEN_TABLE];
        let dt = &d.tables[DIST_TABLE];
        assert_eq!(masked_lookup(llt, 0b00001100), (0, 8));
        assert_eq!(masked_lookup(llt, 0b00011110), (72, 8));
        assert_eq!(masked_lookup(llt, 0b01011110), (74, 8));
        assert_eq!(masked_lookup(llt, 0b11111101), (143, 8));
        assert_eq!(masked_lookup(llt, 0b000010011), (144, 9));
        assert_eq!(masked_lookup(llt, 0b111111111), (255, 9));
        assert_eq!(masked_lookup(llt, 0b00000000), (256, 7));
        assert_eq!(masked_lookup(llt, 0b1110100), (279, 7));
        assert_eq!(masked_lookup(llt, 0b00000011), (280, 8));
        assert_eq!(masked_lookup(llt, 0b11100011), (287, 8));

        assert_eq!(masked_lookup(dt, 0), (0, 5));
        assert_eq!(masked_lookup(dt, 20), (5, 5));
    }

    fn check_result(input: &[u8], status: TINFLStatus, expected_state: State, zlib: bool) {
        let mut r = unsafe { DecompressorOxide::with_init_state_only() };
        let mut output_buf = vec![0; 1024 * 32];
        let mut out_cursor = Cursor::new(output_buf.as_mut_slice());
        let flags = if zlib {
            inflate_flags::TINFL_FLAG_PARSE_ZLIB_HEADER
        } else {
            0
        } | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF;
        let (d_status, _in_bytes, _out_bytes) =
            decompress(&mut r, input, &mut out_cursor, flags);
        assert_eq!(r.state, expected_state);
        assert_eq!(status, d_status);
    }

    #[test]
    fn bogus_input() {
        const F: TINFLStatus = TINFLStatus::Failed;
        // Bad CM.
        check_result(&[0x77, 0x85][..], F, State::BadZlibHeader, true);
        // Bad window size (but check is correct).
        check_result(&[0x88, 0x98][..], F, State::BadZlibHeader, true);
        // Bad check bits.
        check_result(&[0x78, 0x98][..], F, State::BadZlibHeader, true);

        // Too many code lengths. (From inflate library issues)
        check_result(
            b"M\xff\xffM*\xad\xad\xad\xad\xad\xad\xad\xcd\xcd\xcdM",
            F,
            State::BadTotalSymbols,
            false,
        );
        // Bad CLEN (also from inflate library issues)
        check_result(
            b"\xdd\xff\xff*M\x94ffffffffff",
            F,
            State::BadTotalSymbols,
            false,
        );
    }

}
