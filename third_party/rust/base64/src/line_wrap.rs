extern crate safemem;

use super::*;

use std::str;

#[derive(Debug, PartialEq)]
pub struct LineWrapParameters {
    // number of lines that need an ending
    pub lines_with_endings: usize,
    // length of last line (which never needs an ending)
    pub last_line_len: usize,
    // length of lines that need an ending (which are always full lines), with their endings
    pub total_full_wrapped_lines_len: usize,
    // length of all lines, including endings for the ones that need them
    pub total_len: usize,
    // length of the line endings only
    pub total_line_endings_len: usize
}

/// Calculations about how many lines we'll get for a given line length, line ending, etc.
/// This assumes that the last line will not get an ending, even if it is the full line length.
pub fn line_wrap_parameters(input_len: usize, line_len: usize, line_ending: LineEnding)
                            -> LineWrapParameters {
    let line_ending_len = line_ending.len();

    if input_len <= line_len {
        // no wrapping needed
        return LineWrapParameters {
            lines_with_endings: 0,
            last_line_len: input_len,
            total_full_wrapped_lines_len: 0,
            total_len: input_len,
            total_line_endings_len: 0
        };
    };

    // num_lines_with_endings > 0, last_line_length > 0
    let (num_lines_with_endings, last_line_length) = if input_len % line_len > 0 {
        // Every full line has an ending since there is a partial line at the end
        (input_len / line_len, input_len % line_len)
    } else {
        // Every line is a full line, but no trailing ending.
        // Subtraction will not underflow since we know input_len > line_len.
        (input_len / line_len - 1, line_len)
    };

    // TODO should we expose exceeding usize via Result to be kind to 16-bit users? Or is that
    // always going to be a panic anyway in practice? If we choose to use a Result we could pull
    // line wrapping out of the normal encode path and have it be a separate step. Then only users
    // who need line wrapping would care about the possibility for error.

    let single_full_line_with_ending_len = line_len.checked_add(line_ending_len)
        .expect("Line length with ending exceeds usize");
    // length of just the full lines with line endings
    let total_full_wrapped_lines_len = num_lines_with_endings
        .checked_mul(single_full_line_with_ending_len)
        .expect("Full lines with endings length exceeds usize");
    // all lines with appropriate endings, including the last line
    let total_all_wrapped_len = total_full_wrapped_lines_len.checked_add(last_line_length)
        .expect("All lines with endings length exceeds usize");
    let total_line_endings_len = num_lines_with_endings.checked_mul(line_ending_len)
        .expect("Total line endings length exceeds usize");

    LineWrapParameters {
        lines_with_endings: num_lines_with_endings,
        last_line_len: last_line_length,
        total_full_wrapped_lines_len: total_full_wrapped_lines_len,
        total_len: total_all_wrapped_len,
        total_line_endings_len: total_line_endings_len
    }
}


/// Insert line endings into the encoded base64 after each complete line (except the last line, even
/// if it is complete).
/// The provided buffer must be large enough to handle the increased size after endings are
/// inserted.
/// `input_len` is the length of the encoded data in `encoded_buf`.
/// `line_len` is the width without line ending characters.
/// Returns the number of line ending bytes added.
pub fn line_wrap(encoded_buf: &mut [u8], input_len: usize, line_len: usize, line_ending: LineEnding) -> usize {
    let line_wrap_params = line_wrap_parameters(input_len, line_len, line_ending);

    // ptr.offset() is undefined if it wraps, and there is no checked_offset(). However, because
    // we perform this check up front to make sure we have enough capacity, we know that none of
    // the subsequent pointer operations (assuming they implement the desired behavior of course!)
    // will overflow.
    assert!(encoded_buf.len() >= line_wrap_params.total_len,
        "Buffer must be able to hold encoded data after line wrapping");

    // Move the last line, either partial or full, by itself as it does not have a line ending
    // afterwards
    let last_line_start = line_wrap_params.lines_with_endings.checked_mul(line_len)
        .expect("Start of last line in input exceeds usize");
    // last line starts immediately after all the wrapped full lines
    let new_line_start = line_wrap_params.total_full_wrapped_lines_len;

    safemem::copy_over(encoded_buf, last_line_start, new_line_start, line_wrap_params.last_line_len);

    let mut line_ending_bytes = 0;

    let line_ending_len = line_ending.len();

    // handle the full lines
    for line_num in 0..line_wrap_params.lines_with_endings {
        // doesn't underflow because line_num < lines_with_endings
        let lines_before_this_line = line_wrap_params.lines_with_endings - 1 - line_num;
        let old_line_start = lines_before_this_line.checked_mul(line_len)
            .expect("Old line start index exceeds usize");
        let new_line_start = lines_before_this_line.checked_mul(line_ending_len)
            .and_then(|i| i.checked_add(old_line_start))
            .expect("New line start index exceeds usize");

        safemem::copy_over(encoded_buf, old_line_start, new_line_start, line_len);

        let after_new_line = new_line_start.checked_add(line_len)
            .expect("Line ending index exceeds usize");

        match line_ending {
            LineEnding::LF => {
                encoded_buf[after_new_line] = b'\n';
                line_ending_bytes += 1;
            }
            LineEnding::CRLF => {
                encoded_buf[after_new_line] = b'\r';
                encoded_buf[after_new_line.checked_add(1).expect("Line ending index exceeds usize")]
                    = b'\n';
                line_ending_bytes += 2;
            }
        }
    }

    assert_eq!(line_wrap_params.total_line_endings_len, line_ending_bytes);

    line_ending_bytes
}

#[cfg(test)]
mod tests {
    extern crate rand;

    use super::super::*;
    use super::*;

    use self::rand::Rng;
    use self::rand::distributions::{IndependentSample, Range};

    #[test]
    fn line_params_perfect_multiple_of_line_length_lf() {
        let params = line_wrap_parameters(100, 20, LineEnding::LF);

        assert_eq!(LineWrapParameters {
            lines_with_endings: 4,
            last_line_len: 20,
            total_full_wrapped_lines_len: 84,
            total_len: 104,
            total_line_endings_len: 4
        }, params);
    }

    #[test]
    fn line_params_partial_last_line_crlf() {
        let params = line_wrap_parameters(103, 20, LineEnding::CRLF);

        assert_eq!(LineWrapParameters {
            lines_with_endings: 5,
            last_line_len: 3,
            total_full_wrapped_lines_len: 110,
            total_len: 113,
            total_line_endings_len: 10
        }, params);
    }

    #[test]
    fn line_params_line_len_1_crlf() {
        let params = line_wrap_parameters(100, 1, LineEnding::CRLF);

        assert_eq!(LineWrapParameters {
            lines_with_endings: 99,
            last_line_len: 1,
            total_full_wrapped_lines_len: 99 * 3,
            total_len: 99 * 3 + 1,
            total_line_endings_len: 99 * 2
        }, params);
    }

    #[test]
    fn line_params_line_len_longer_than_input_crlf() {
        let params = line_wrap_parameters(100, 200, LineEnding::CRLF);

        assert_eq!(LineWrapParameters {
            lines_with_endings: 0,
            last_line_len: 100,
            total_full_wrapped_lines_len: 0,
            total_len: 100,
            total_line_endings_len: 0
        }, params);
    }

    #[test]
    fn line_params_line_len_same_as_input_crlf() {
        let params = line_wrap_parameters(100, 100, LineEnding::CRLF);

        assert_eq!(LineWrapParameters {
            lines_with_endings: 0,
            last_line_len: 100,
            total_full_wrapped_lines_len: 0,
            total_len: 100,
            total_line_endings_len: 0
        }, params);
    }

    #[test]
    fn line_wrap_length_1_lf() {
        let mut buf = vec![0x1, 0x2, 0x3, 0x4];

        do_line_wrap(&mut buf, 1, LineEnding::LF);

        assert_eq!(vec![0x1, 0xA, 0x2, 0xA, 0x3, 0xA, 0x4], buf);
    }

    #[test]
    fn line_wrap_length_1_crlf() {
        let mut buf = vec![0x1, 0x2, 0x3, 0x4];

        do_line_wrap(&mut buf, 1, LineEnding::CRLF);

        assert_eq!(vec![0x1, 0xD, 0xA, 0x2, 0xD, 0xA, 0x3, 0xD, 0xA, 0x4], buf);
    }

    #[test]
    fn line_wrap_length_2_lf_full_lines() {
        let mut buf = vec![0x1, 0x2, 0x3, 0x4];

        do_line_wrap(&mut buf, 2, LineEnding::LF);

        assert_eq!(vec![0x1, 0x2, 0xA, 0x3, 0x4], buf);
    }

    #[test]
    fn line_wrap_length_2_crlf_full_lines() {
        let mut buf = vec![0x1, 0x2, 0x3, 0x4];

        do_line_wrap(&mut buf, 2, LineEnding::CRLF);

        assert_eq!(vec![0x1, 0x2, 0xD, 0xA, 0x3, 0x4], buf);
    }

    #[test]
    fn line_wrap_length_2_lf_partial_line() {
        let mut buf = vec![0x1, 0x2, 0x3, 0x4, 0x5];

        do_line_wrap(&mut buf, 2, LineEnding::LF);

        assert_eq!(vec![0x1, 0x2, 0xA, 0x3, 0x4, 0xA, 0x5], buf);
    }

    #[test]
    fn line_wrap_length_2_crlf_partial_line() {
        let mut buf = vec![0x1, 0x2, 0x3, 0x4, 0x5];

        do_line_wrap(&mut buf, 2, LineEnding::CRLF);

        assert_eq!(vec![0x1, 0x2, 0xD, 0xA, 0x3, 0x4, 0xD, 0xA, 0x5], buf);
    }

    #[test]
    fn line_wrap_random() {
        let mut buf: Vec<u8> = Vec::new();
        let buf_range = Range::new(10, 1000);
        let line_range = Range::new(10, 100);
        let mut rng = rand::weak_rng();

        for _ in 0..10_000 {
            buf.clear();

            let buf_len = buf_range.ind_sample(&mut rng);
            let line_len = line_range.ind_sample(&mut rng);
            let line_ending = if rng.gen() {
                LineEnding::LF
            } else {
                LineEnding::CRLF
            };
            let line_ending_len = line_ending.len();

            for _ in 0..buf_len {
                buf.push(rng.gen());
            }

            let line_wrap_params = line_wrap_parameters(buf_len, line_len, line_ending);

            let not_wrapped_buf = buf.to_vec();

            let _ = do_line_wrap(&mut buf, line_len, line_ending);

            // remove the endings
            for line_ending_num in 0..line_wrap_params.lines_with_endings {
                let line_ending_offset = (line_ending_num + 1) * line_len;

                for _ in 0..line_ending_len {
                    buf.remove(line_ending_offset);
                }
            }

            assert_eq!(not_wrapped_buf, buf);
        }
    }

    fn do_line_wrap(buf: &mut Vec<u8>, line_len: usize, line_ending: LineEnding) -> usize {
        let mut rng = rand::weak_rng();

        let orig_len = buf.len();

        // A 3x inflation is enough for the worst case: line length 1, crlf ending.
        // We add on extra bytes so we'll have un-wrapped bytes at the end that shouldn't get
        // modified..
        for _ in 0..(1000 + 2 * orig_len) {
            buf.push(rng.gen());
        }

        let mut before_line_wrap = buf.to_vec();

        let params = line_wrap_parameters(orig_len, line_len, line_ending);

        let bytes_written = line_wrap(&mut buf[..], orig_len, line_len, line_ending);

        assert_eq!(params.total_line_endings_len, bytes_written);
        assert_eq!(params.lines_with_endings * line_ending.len(), bytes_written);
        assert_eq!(params.total_len, orig_len + bytes_written);

        // make sure line_wrap didn't touch anything beyond what it should
        let start_of_untouched_data = orig_len + bytes_written;
        assert_eq!(before_line_wrap[start_of_untouched_data..],
            buf[start_of_untouched_data..]);

        // also make sure that line wrapping will fit into a slice no bigger than what it should
        // need
        let bytes_written_precise_fit =
            line_wrap(&mut before_line_wrap[0..(params.total_len)], orig_len, line_len,
                      line_ending);

        assert_eq!(bytes_written, bytes_written_precise_fit);
        assert_eq!(&buf[0..(params.total_len)], &before_line_wrap[0..(params.total_len)]);

        buf.truncate(orig_len + bytes_written);

        bytes_written
    }
}
