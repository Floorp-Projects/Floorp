use std::iter::Iterator;
use std::clone::Clone;

/// An enum representing the different types in the run-length encoded data used to encode
/// huffman table lengths
#[derive(Debug, PartialEq, Eq)]
pub enum EncodedLength {
    // An actual length value
    Length(u8),
    // Copy the previous value n times
    CopyPrevious(u8),
    // Repeat zero n times (with n represented by 3 bits)
    RepeatZero3Bits(u8),
    // Repeat zero n times (with n represented by 7 bits)
    RepeatZero7Bits(u8),
}

impl EncodedLength {
    fn from_prev_and_repeat(prev: u8, repeat: u8) -> EncodedLength {
        match prev {
            0 => {
                if repeat <= 10 {
                    EncodedLength::RepeatZero3Bits(repeat)
                } else {
                    EncodedLength::RepeatZero7Bits(repeat)
                }
            }
            1...15 => EncodedLength::CopyPrevious(repeat),
            _ => panic!(),
        }
    }
}

pub const COPY_PREVIOUS: usize = 16;
pub const REPEAT_ZERO_3_BITS: usize = 17;
pub const REPEAT_ZERO_7_BITS: usize = 18;

const MIN_REPEAT: u8 = 3;

/// Push an `EncodedLength` to the vector and update the frequency table.
fn update_out_and_freq(
    encoded: EncodedLength,
    output: &mut Vec<EncodedLength>,
    frequencies: &mut [u16; 19],
) {
    let index = match encoded {
        EncodedLength::Length(l) => usize::from(l),
        EncodedLength::CopyPrevious(_) => COPY_PREVIOUS,
        EncodedLength::RepeatZero3Bits(_) => REPEAT_ZERO_3_BITS,
        EncodedLength::RepeatZero7Bits(_) => REPEAT_ZERO_7_BITS,
    };

    frequencies[index] += 1;

    output.push(encoded);
}

/// Convenience function to check if the repeat counter should be incremented further
fn not_max_repetitions(length_value: u8, repeats: u8) -> bool {
    (length_value == 0 && repeats < 138) || repeats < 6
}

///Convenience version for unit tests.
#[cfg(test)]
pub fn encode_lengths<'a, I>(lengths: I) -> (Vec<EncodedLength>, [u16; 19])
where
    I: Iterator<Item = &'a u8> + Clone,
{
    let mut freqs = [0u16; 19];
    let mut encoded: Vec<EncodedLength> = Vec::new();
    encode_lengths_m(lengths, &mut encoded, &mut freqs);
    (encoded, freqs)
}

/// Run-length encodes the lengths of the values in `lengths` according to the deflate
/// specification. This is used for writing the code lengths for the huffman tables for
/// the deflate stream.
///
/// Returns a tuple containing a vec of the encoded lengths
/// Populates the supplied array with the frequency of the different encoded length values
/// The frequency array is taken as a parameter rather than returned to avoid
/// excessive memcpying.
pub fn encode_lengths_m<'a, I>(
    lengths: I,
    mut out: &mut Vec<EncodedLength>,
    mut frequencies: &mut [u16; 19],
) where
    I: Iterator<Item = &'a u8> + Clone,
{
    out.clear();
    // Number of repetitions of the current value
    let mut repeat = 0;
    let mut iter = lengths.clone().enumerate().peekable();
    // Previous value
    // We set it to the compliment of the first falue to simplify the code.
    let mut prev = !iter.peek().expect("No length values!").1;

    while let Some((n, &l)) = iter.next() {
        if l == prev && not_max_repetitions(l, repeat) {
            repeat += 1;
        }
        if l != prev || iter.peek().is_none() || !not_max_repetitions(l, repeat) {
            if repeat >= MIN_REPEAT {
                // The previous value has been repeated enough times to write out a repeat code.

                let val = EncodedLength::from_prev_and_repeat(prev, repeat);
                update_out_and_freq(val, &mut out, &mut frequencies);
                repeat = 0;
                // If we have a new length value, output l unless the last value is 0 or l is the
                // last byte.
                if l != prev {
                    if l != 0 || iter.peek().is_none() {
                        update_out_and_freq(EncodedLength::Length(l), &mut out, &mut frequencies);
                        repeat = 0;
                    } else {
                        // If we have a zero, we start repeat at one instead of outputting, as
                        // there are separate codes for repeats of zero so we don't need a literal
                        // to define what byte to repeat.
                        repeat = 1;
                    }
                }
            } else {
                // There haven't been enough repetitions of the previous value,
                // so just we output the lengths directly.

                // If we are at the end, and we have a value that is repeated, we need to
                // skip a byte and output the last one.
                let extra_skip = if iter.peek().is_none() && l == prev {
                    1
                } else {
                    0
                };

                // Get to the position of the next byte to output by starting at zero and skipping.
                let b_iter = lengths.clone().skip(n + extra_skip - repeat as usize);

                // As repeats of zeroes have separate codes, we don't need to output a literal here
                // if we have a zero (unless we are at the end).
                let extra = if l != 0 || iter.peek().is_none() {
                    1
                } else {
                    0
                };

                for &i in b_iter.take(repeat as usize + extra) {
                    update_out_and_freq(EncodedLength::Length(i), &mut out, &mut frequencies);
                }

                // If the current byte is zero we start repeat at 1 as we didn't output the literal
                // directly.
                repeat = 1 - extra as u8;
            }
        }
        prev = l;
    }
}

#[cfg(test)]
pub fn huffman_lengths_from_frequency(frequencies: &[u16], max_len: usize) -> Vec<u8> {
    in_place::gen_lengths(frequencies, max_len)
}

pub type LeafVec = Vec<in_place::Node>;

/// Generate a set of canonical huffman lengths from the given frequencies, with a maximum length
/// of `max_len`. The lengths are put in the lens slice parameter. Unused lengths are set to 0.
///
/// The leaf buffer is passed in to avoid allocating it every time this function is called.
/// The existing data contained in it is not preserved.
pub fn huffman_lengths_from_frequency_m(
    frequencies: &[u16],
    max_len: usize,
    leaf_buffer: &mut LeafVec,
    lens: &mut [u8],
) {
    in_place::in_place_lengths(frequencies, max_len, leaf_buffer, lens);
}

mod in_place {
    type WeightType = u32;

    pub fn validate_lengths(lengths: &[u8]) -> bool {
        // Avoid issue with floating point on mips: https://github.com/oyvindln/deflate-rs/issues/23
        if cfg!(any(target_arch = "mips", target_arch = "mipsel",
                    target_arch = "mips64", target_arch = "mipsel64")) {
            true
        } else {
            let v = lengths.iter().fold(0f64, |acc, &n| {
                acc + if n != 0 { 2f64.powi(-(n as i32)) } else { 0f64 }
            });
            !(v > 1.0)
        }
    }

    #[derive(Eq, PartialEq, Debug)]
    pub struct Node {
        value: WeightType,
        symbol: u16,
    }

    fn step_1(leaves: &mut [Node]) {
        // If there are less than 2 non-zero frequencies, this function should not have been
        // called and we should not have gotten to this point.
        debug_assert!(leaves.len() >= 2);
        let mut root = 0;
        let mut leaf = 2;

        leaves[0].value += leaves[1].value;

        for next in 1..leaves.len() - 1 {
            if (leaf >= leaves.len()) || (leaves[root].value < leaves[leaf].value) {
                leaves[next].value = leaves[root].value;
                leaves[root].value = next as WeightType;
                root += 1;
            } else {
                leaves[next].value = leaves[leaf].value;
                leaf += 1;
            }

            if (leaf >= leaves.len()) ||
                (root < next && (leaves[root].value < leaves[leaf].value))
            {
                leaves[next].value += leaves[root].value;
                leaves[root].value = next as WeightType;
                root += 1;
            } else {
                leaves[next].value += leaves[leaf].value;
                leaf += 1;
            }
        }
    }

    fn step_2(leaves: &mut [Node]) {
        debug_assert!(leaves.len() >= 2);
        let n = leaves.len();

        leaves[n - 2].value = 0;
        for t in (0..(n + 1 - 3)).rev() {
            leaves[t].value = leaves[leaves[t].value as usize].value + 1;
        }

        let mut available = 1 as usize;
        let mut used = 0;
        let mut depth = 0;
        let mut root = n as isize - 2;
        let mut next = n as isize - 1;

        while available > 0 {
            while root >= 0 && leaves[root as usize].value == depth {
                used += 1;
                root -= 1;
            }
            while available > used {
                leaves[next as usize].value = depth;
                next -= 1;
                available -= 1;
            }
            available = 2 * used;
            depth += 1;
            used = 0;
        }
    }

    const MAX_NUMBER_OF_CODES: usize = 32;
    const NUM_CODES_LENGTH: usize = MAX_NUMBER_OF_CODES + 1;

    /// Checks if any of the lengths exceed `max_len`, and if that is the case, alters the length
    /// table so that no codes exceed `max_len`.
    /// This is ported from miniz (which is released as public domain by Rich Geldreich
    /// https://github.com/richgel999/miniz/blob/master/miniz.c)
    ///
    /// This will not generate optimal (minimim-redundancy) codes, however in most cases
    /// this won't make a large difference.
    pub fn enforce_max_code_lengths(
        num_codes: &mut [u16; NUM_CODES_LENGTH],
        num_used: usize,
        max_len: usize,
    ) {
        debug_assert!(max_len <= 15);

        if num_used <= 1 {
            return;
        } else {
            let mut num_above_max = 0u16;
            for &l in num_codes[(max_len as usize + 1)..].iter() {
                num_above_max += l;
            }

            num_codes[max_len] += num_above_max;

            let mut total = 0u32;
            for i in (1..max_len + 1).rev() {
                // This should be safe as max_len won't be higher than 15, and num_codes[i] can't
                // be higher than 288,
                // and 288 << 15 will not be anywhere close to overflowing 32 bits
                total += (num_codes[i] as u32) << (max_len - i);
            }

            // miniz uses unsigned long here. 32-bits should be sufficient though,
            // as max_len won't be longer than 15 anyhow.
            while total != 1u32 << max_len {
                num_codes[max_len] -= 1;
                for i in (1..max_len).rev() {
                    if num_codes[i] != 0 {
                        num_codes[i] -= 1;
                        num_codes[i + 1] += 2;
                        break;
                    }
                }
                total -= 1;
            }
        }
    }


    #[cfg(test)]
    /// Convenience wrapper for tests.
    pub fn gen_lengths(frequencies: &[u16], max_len: usize) -> Vec<u8> {
        let mut lens = vec![0u8; frequencies.len()];
        let mut leaves = Vec::new();
        in_place_lengths(frequencies, max_len, &mut leaves, lens.as_mut_slice());
        lens
    }

    /// Generate huffman code lengths, using the algorithm described by
    /// Moffat and Katajainen in In-Place Calculation of Minimum-Redundancy Codes
    /// http://people.eng.unimelb.edu.au/ammoffat/abstracts/mk95wads.html
    /// and it's implementation.
    ///
    /// This is significantly faster, and seems to generally create lengths that result in length
    /// tables that are better compressible than the algorithm used previously. The downside of this
    /// algorithm is that it's not length-limited, so if too long code lengths are generated,
    /// it might result in a sub-optimal tables as the length-restricting function isn't optimal.
    pub fn in_place_lengths(
        frequencies: &[u16],
        max_len: usize,
        mut leaves: &mut Vec<Node>,
        lengths: &mut [u8],
    ) {

        debug_assert!(lengths.len() >= frequencies.len());

        for l in lengths.iter_mut() {
            *l = 0;
        }

        // Clear any previous leaves in the leaf buffer.
        leaves.clear();

        // Discard zero length nodes as they won't be given a code and thus don't need to
        // participate in code length generation and create a new vec of the remaining
        // symbols and weights.
        leaves.extend(frequencies.iter().enumerate().filter_map(
            |(n, f)| if *f > 0 {
                Some(Node {
                    value: *f as WeightType,
                    symbol: n as u16,
                })
            } else {
                None
            },
        ));

        // Special cases with zero or 1 value having a non-zero frequency
        if leaves.len() == 1 {
            lengths[leaves[0].symbol as usize] = 1;
            return;
        } else if leaves.is_empty() {
            return;
        }

        // Sort the leaves by value. As the sort in the standard library is stable, we don't
        // have to worry about the symbol code here.
        leaves.sort_by(|a, b| a.value.cmp(&b.value));

        step_1(&mut leaves);
        step_2(&mut leaves);

        // Count how many codes of each length used, for usage in the next section.
        let mut num_codes = [0u16; NUM_CODES_LENGTH];
        for l in leaves.iter() {
            num_codes[l.value as usize] += 1;
        }

        // As the algorithm used here doesn't limit the maximum length that can be generated
        // we need to make sure none of the lengths exceed `max_len`
        enforce_max_code_lengths(&mut num_codes, leaves.len(), max_len);

        // Output the actual lengths
        let mut leaf_it = leaves.iter().rev();
        // Start at 1 since the length table is already filled with zeroes.
        for (&n_codes, i) in num_codes[1..max_len + 1].iter().zip(1..(max_len as u8) + 1) {
            for _ in 0..n_codes {
                lengths[leaf_it.next().unwrap().symbol as usize] = i;
            }
        }

        debug_assert_eq!(leaf_it.next(), None);
        debug_assert!(
            validate_lengths(lengths),
            "The generated length codes were not valid!"
        );
    }


}

#[cfg(test)]
mod test {
    use super::*;
    use std::u16;
    use huffman_table::NUM_LITERALS_AND_LENGTHS;

    fn lit(value: u8) -> EncodedLength {
        EncodedLength::Length(value)
    }

    fn zero(repeats: u8) -> EncodedLength {
        match repeats {
            0...1 => EncodedLength::Length(0),
            2...10 => EncodedLength::RepeatZero3Bits(repeats),
            _ => EncodedLength::RepeatZero7Bits(repeats),
        }
    }

    fn copy(copies: u8) -> EncodedLength {
        EncodedLength::CopyPrevious(copies)
    }

    #[test]
    fn test_encode_lengths() {
        use huffman_table::FIXED_CODE_LENGTHS;
        let enc = encode_lengths(FIXED_CODE_LENGTHS.iter());
        // There are no lengths lower than 6 in the fixed table
        assert_eq!(enc.1[0..7], [0, 0, 0, 0, 0, 0, 0]);
        // Neither are there any lengths above 9
        assert_eq!(enc.1[10..16], [0, 0, 0, 0, 0, 0]);
        // Also there are no zero-length codes so there shouldn't be any repetitions of zero
        assert_eq!(enc.1[17..19], [0, 0]);

        let test_lengths = [0, 0, 5, 0, 15, 1, 0, 0, 0, 2, 4, 4, 4, 4, 3, 5, 5, 5, 5];
        let enc = encode_lengths(test_lengths.iter()).0;
        assert_eq!(
            enc,
            vec![
                lit(0),
                lit(0),
                lit(5),
                lit(0),
                lit(15),
                lit(1),
                zero(3),
                lit(2),
                lit(4),
                copy(3),
                lit(3),
                lit(5),
                copy(3),
            ]
        );
        let test_lengths = [0, 0, 0, 5, 2, 3, 0, 0, 0];
        let enc = encode_lengths(test_lengths.iter()).0;
        assert_eq!(enc, vec![zero(3), lit(5), lit(2), lit(3), zero(3)]);

        let test_lengths = [0, 0, 0, 3, 3, 3, 5, 4, 4, 4, 4, 0, 0];
        let enc = encode_lengths(test_lengths.iter()).0;
        assert_eq!(
            enc,
            vec![
                zero(3),
                lit(3),
                lit(3),
                lit(3),
                lit(5),
                lit(4),
                copy(3),
                lit(0),
                lit(0),
            ]
        );


        let lens = [
            0,
            0,
            4,
            0,
            0,
            4,
            0,
            0,
            0,
            0,
            0,
            4,
            4,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            3,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            4,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            4,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
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
        ];

        let _ = encode_lengths(lens.iter()).0;

        let lens = [
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            9,
            0,
            0,
            9,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            6,
            0,
            0,
            0,
            8,
            0,
            0,
            0,
            0,
            8,
            0,
            0,
            7,
            8,
            7,
            8,
            6,
            6,
            8,
            0,
            7,
            6,
            7,
            8,
            7,
            7,
            8,
            0,
            0,
            0,
            0,
            0,
            8,
            8,
            0,
            8,
            7,
            0,
            10,
            8,
            0,
            8,
            0,
            10,
            10,
            8,
            8,
            10,
            8,
            0,
            8,
            7,
            0,
            10,
            0,
            7,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            6,
            7,
            7,
            7,
            6,
            7,
            8,
            8,
            6,
            0,
            0,
            8,
            8,
            7,
            8,
            8,
            0,
            7,
            6,
            6,
            8,
            8,
            8,
            10,
            10,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            10,
            4,
            3,
            3,
            4,
            4,
            5,
            5,
            5,
            5,
            5,
            8,
            8,
            6,
            7,
            8,
            10,
            10,
            0,
            9, /* litlen */
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            8,
            8,
            8,
            8,
            6,
            6,
            5,
            5,
            5,
            5,
            6,
            5,
            5,
            4,
            4,
            4,
            4,
            4,
            4,
            3,
            4,
            3,
            4,
        ];

        let enc = encode_lengths(lens.iter()).0;

        assert_eq!(
            &enc[..10],
            &[
                zero(10),
                lit(9),
                lit(0),
                lit(0),
                lit(9),
                zero(18),
                lit(6),
                zero(3),
                lit(8),
                zero(4)
            ]
        );
        assert_eq!(
            &enc[10..20],
            &[
                lit(8),
                lit(0),
                lit(0),
                lit(7),
                lit(8),
                lit(7),
                lit(8),
                lit(6),
                lit(6),
                lit(8)
            ]
        );

        let enc = encode_lengths([1, 1, 1, 2].iter()).0;
        assert_eq!(enc, vec![lit(1), lit(1), lit(1), lit(2)]);
        let enc = encode_lengths([0, 0, 3].iter()).0;
        assert_eq!(enc, vec![lit(0), lit(0), lit(3)]);
        let enc = encode_lengths([0, 0, 0, 5, 2].iter()).0;
        assert_eq!(enc, vec![zero(3), lit(5), lit(2)]);

        let enc = encode_lengths([0, 0, 0, 5, 0].iter()).0;
        assert!(*enc.last().unwrap() != lit(5));

        let enc = encode_lengths([0, 4, 4, 4, 4, 0].iter()).0;
        assert_eq!(*enc.last().unwrap(), zero(0));
    }

    #[test]
    fn test_lengths_from_frequencies() {
        let frequencies = [1, 1, 5, 7, 10, 14];

        let expected = [4, 4, 3, 2, 2, 2];
        let res = huffman_lengths_from_frequency(&frequencies, 4);

        assert_eq!(expected, res.as_slice());

        let frequencies = [1, 5, 1, 7, 10, 14];
        let expected = [4, 3, 4, 2, 2, 2];

        let res = huffman_lengths_from_frequency(&frequencies, 4);

        assert_eq!(expected, res.as_slice());

        let frequencies = [0, 25, 0, 10, 2, 4];

        let res = huffman_lengths_from_frequency(&frequencies, 4);
        assert_eq!(res[0], 0);
        assert_eq!(res[2], 0);
        assert!(res[1] < 4);

        // Only one value
        let frequencies = [0, 0, 0, 0, 0, 0, 0, 0, 55, 0, 0, 0];
        let res = huffman_lengths_from_frequency(&frequencies, 5);
        let expected = [0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0];
        assert_eq!(expected, res.as_slice());

        // No values
        let frequencies = [0; 30];
        let res = huffman_lengths_from_frequency(&frequencies, 5);
        for (a, b) in frequencies.iter().zip(res.iter()) {
            assert_eq!(*a, (*b).into());
        }
        // assert_eq!(frequencies, res.as_slice());

        let mut frequencies = vec![3; NUM_LITERALS_AND_LENGTHS];
        frequencies[55] = u16::MAX / 3;
        frequencies[125] = u16::MAX / 3;

        let res = huffman_lengths_from_frequency(&frequencies, 15);
        assert_eq!(res.len(), NUM_LITERALS_AND_LENGTHS);
        assert!(res[55] < 3);
        assert!(res[125] < 3);
    }

    #[test]
    /// Test if the bit lengths for a set of frequencies are optimal (give the best compression
    /// give the provided frequencies).
    fn optimal_lengths() {
        let freqs = [
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            44,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            68,
            0,
            14,
            0,
            0,
            0,
            0,
            3,
            7,
            6,
            1,
            0,
            12,
            14,
            9,
            2,
            6,
            9,
            4,
            1,
            1,
            4,
            1,
            1,
            0,
            0,
            1,
            3,
            0,
            6,
            0,
            0,
            0,
            4,
            4,
            1,
            2,
            5,
            3,
            2,
            2,
            9,
            0,
            0,
            3,
            1,
            5,
            5,
            8,
            0,
            6,
            10,
            5,
            2,
            0,
            0,
            1,
            2,
            0,
            8,
            11,
            4,
            0,
            1,
            3,
            31,
            13,
            23,
            22,
            56,
            22,
            8,
            11,
            43,
            0,
            7,
            33,
            15,
            45,
            40,
            16,
            1,
            28,
            37,
            35,
            26,
            3,
            7,
            11,
            9,
            1,
            1,
            0,
            1,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            1,
            126,
            114,
            66,
            31,
            41,
            25,
            15,
            21,
            20,
            16,
            15,
            10,
            7,
            5,
            1,
            1,
        ];


        let lens = huffman_lengths_from_frequency(&freqs, 15);

        // Lengths produced by miniz for this frequency table for comparison
        // the number of total bits encoded with these huffman codes is 7701
        // NOTE: There can be more than one set of optimal lengths for a set of frequencies,
        // (though there may be a difference in how well the table itself can be represented)
        // so testing for a specific length table is not ideal since different algorithms
        // may produce different length tables.
        // let lens3 = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        // 0, 0, 0, 0, 0,
        // 0, 0, 0, 0, 0, 0, 4, 0, 7, 0, 0, 0, 0, 9, 8, 8, 10, 0, 7, 7, 7, 10, 8, 7, 8,
        // 10, 10, 8, 10, 10, 0, 0, 10, 9, 0, 8, 0, 0, 0, 8, 8, 10, 9, 8, 9, 9, 9, 7, 0,
        // 0, 9, 10, 8, 8, 7, 0, 8, 7, 8, 9, 0, 0, 10, 9, 0, 7, 7, 8, 0, 10, 9, 6, 7, 6,
        // 6, 5, 6, 7, 7, 5, 0, 8, 5, 7, 5, 5, 6, 10, 6, 5, 5, 6, 9, 8, 7, 7, 10, 10, 0,
        // 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        // 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        // 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        // 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        // 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        // 0, 0, 10, 4, 4, 4, 5, 5, 6, 7, 6, 6, 6, 6, 7, 8, 8, 10, 10];
        //


        let num_bits = lens.iter()
            .zip(freqs.iter())
            .fold(0, |a, (&f, &l)| a + (f as u16 * l));
        assert_eq!(num_bits, 7701);
    }
}
