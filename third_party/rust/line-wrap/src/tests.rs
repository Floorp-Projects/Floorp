extern crate rand;


use super::*;
use self::rand::distributions::{Distribution, Range};
use self::rand::{Rng, FromEntropy};

#[test]
fn line_params_perfect_multiple_of_line_length_lf() {
    let params = line_wrap_parameters(100, 20, lf().len());

    assert_eq!(
        LineWrapParameters {
            line_with_ending_len: 21,
            lines_with_endings: 4,
            last_line_len: 20,
            total_full_wrapped_lines_len: 84,
            total_len: 104,
            total_line_endings_len: 4,
        },
        params
    );
}

#[test]
fn line_params_partial_last_line_crlf() {
    let params = line_wrap_parameters(103, 20, crlf().len());

    assert_eq!(
        LineWrapParameters {
            line_with_ending_len: 22,
            lines_with_endings: 5,
            last_line_len: 3,
            total_full_wrapped_lines_len: 110,
            total_len: 113,
            total_line_endings_len: 10,
        },
        params
    );
}

#[test]
fn line_params_line_len_1_crlf() {
    let params = line_wrap_parameters(100, 1, crlf().len());

    assert_eq!(
        LineWrapParameters {
            line_with_ending_len: 3,
            lines_with_endings: 99,
            last_line_len: 1,
            total_full_wrapped_lines_len: 99 * 3,
            total_len: 99 * 3 + 1,
            total_line_endings_len: 99 * 2,
        },
        params
    );
}

#[test]
fn line_params_line_len_longer_than_input_crlf() {
    let params = line_wrap_parameters(100, 200, crlf().len());

    assert_eq!(
        LineWrapParameters {
            line_with_ending_len: 202,
            lines_with_endings: 0,
            last_line_len: 100,
            total_full_wrapped_lines_len: 0,
            total_len: 100,
            total_line_endings_len: 0,
        },
        params
    );
}

#[test]
fn line_params_line_len_same_as_input_crlf() {
    let params = line_wrap_parameters(100, 100, crlf().len());

    assert_eq!(
        LineWrapParameters {
            line_with_ending_len: 102,
            lines_with_endings: 0,
            last_line_len: 100,
            total_full_wrapped_lines_len: 0,
            total_len: 100,
            total_line_endings_len: 0,
        },
        params
    );
}


#[test]
fn line_wrap_length_1_lf() {
    let mut buf = vec![0x1, 0x2, 0x3, 0x4];

    assert_eq!(3, do_line_wrap(&mut buf, 1, &lf()));

    assert_eq!(vec![0x1, 0xA, 0x2, 0xA, 0x3, 0xA, 0x4], buf);
}

#[test]
fn line_wrap_length_1_crlf() {
    let mut buf = vec![0x1, 0x2, 0x3, 0x4];

    assert_eq!(6, do_line_wrap(&mut buf, 1, &crlf()));

    assert_eq!(vec![0x1, 0xD, 0xA, 0x2, 0xD, 0xA, 0x3, 0xD, 0xA, 0x4], buf);
}

#[test]
fn line_wrap_length_2_lf_full_lines() {
    let mut buf = vec![0x1, 0x2, 0x3, 0x4];

    assert_eq!(1, do_line_wrap(&mut buf, 2, &lf()));

    assert_eq!(vec![0x1, 0x2, 0xA, 0x3, 0x4], buf);
}

#[test]
fn line_wrap_length_2_crlf_full_lines() {
    let mut buf = vec![0x1, 0x2, 0x3, 0x4];

    assert_eq!(2, do_line_wrap(&mut buf, 2, &crlf()));

    assert_eq!(vec![0x1, 0x2, 0xD, 0xA, 0x3, 0x4], buf);
}

#[test]
fn line_wrap_length_2_lf_partial_line() {
    let mut buf = vec![0x1, 0x2, 0x3, 0x4, 0x5];

    assert_eq!(2, do_line_wrap(&mut buf, 2, &lf()));

    assert_eq!(vec![0x1, 0x2, 0xA, 0x3, 0x4, 0xA, 0x5], buf);
}

#[test]
fn line_wrap_length_2_crlf_partial_line() {
    let mut buf = vec![0x1, 0x2, 0x3, 0x4, 0x5];

    assert_eq!(4, do_line_wrap(&mut buf, 2, &crlf()));

    assert_eq!(vec![0x1, 0x2, 0xD, 0xA, 0x3, 0x4, 0xD, 0xA, 0x5], buf);
}

#[test]
fn line_wrap_random_slice() {
    let mut buf: Vec<u8> = Vec::new();
    let buf_range = Range::new(10, 1000);
    let line_range = Range::new(10, 100);
    let mut rng = rand::rngs::SmallRng::from_entropy();
    let mut line_ending_bytes: Vec<u8> = Vec::new();

    for _ in 0..10_000 {
        buf.clear();
        line_ending_bytes.clear();

        let buf_len = buf_range.sample(&mut rng);
        let line_len = line_range.sample(&mut rng);

        for _ in 0..buf_len {
            buf.push(rng.gen());
        }

        line_ending_bytes.clear();
        for _ in 0..rng.gen_range(1, 11) {
            line_ending_bytes.push(rng.gen());
        }
        let line_ending = SliceLineEnding::new(&line_ending_bytes);

        let _ = do_line_wrap(&mut buf, line_len, &line_ending);
    }
}

#[test]
fn line_wrap_random_single_byte() {
    let mut buf: Vec<u8> = Vec::new();
    let buf_range = Range::new(10, 1000);
    let line_range = Range::new(10, 100);
    let mut rng = rand::rngs::SmallRng::from_entropy();

    for _ in 0..10_000 {
        buf.clear();

        let buf_len = buf_range.sample(&mut rng);
        let line_len = line_range.sample(&mut rng);

        for _ in 0..buf_len {
            buf.push(rng.gen());
        }

        let line_ending = ByteLineEnding::new(rng.gen());

        let _ = do_line_wrap(&mut buf, line_len, &line_ending);
    }
}

#[test]
fn line_wrap_random_two_bytes() {
    let mut buf: Vec<u8> = Vec::new();
    let buf_range = Range::new(10, 1000);
    let line_range = Range::new(10, 100);
    let mut rng = rand::rngs::SmallRng::from_entropy();

    for _ in 0..10_000 {
        buf.clear();

        let buf_len = buf_range.sample(&mut rng);
        let line_len = line_range.sample(&mut rng);

        for _ in 0..buf_len {
            buf.push(rng.gen());
        }

        let line_ending = TwoByteLineEnding::new(rng.gen(), rng.gen());

        let _ = do_line_wrap(&mut buf, line_len, &line_ending);
    }
}

fn do_line_wrap<L: LineEnding>(buf: &mut Vec<u8>, line_len: usize, line_ending: &L) -> usize {
    let mut rng = rand::rngs::SmallRng::from_entropy();

    let orig_len = buf.len();
    let orig_buf = buf.to_vec();

    // Add on extra bytes so we'll have sentinel bytes at the end that shouldn't get changed.
    for _ in 0..(1000 + line_ending.len() * orig_len) {
        buf.push(rng.gen());
    }

    let before_line_wrap = buf.to_vec();

    let params = line_wrap_parameters(orig_len, line_len, line_ending.len());

    let bytes_written = line_wrap::<L>(&mut buf[..], orig_len, line_len, line_ending);

    assert_eq!(params.total_line_endings_len, bytes_written);
    assert_eq!(params.lines_with_endings * line_ending.len(), bytes_written);
    assert_eq!(params.total_len, orig_len + bytes_written);

    // make sure line_wrap didn't touch anything beyond what it should
    assert_eq!(before_line_wrap[params.total_len..], buf[params.total_len..]);

    {
        // also make sure that line wrapping will fit into a slice no bigger than what it should
        // need
        let mut buf_strict = before_line_wrap.to_vec();
        buf_strict.truncate(orig_len);
        // fill in some fresh random garbage
        while buf_strict.len() < params.total_len {
            buf_strict.push(rng.gen());
        }
        let bytes_written_precise_fit = line_wrap::<L>(
            &mut buf_strict,
            orig_len,
            line_len,
            line_ending,
        );
        assert_eq!(params.total_len, buf_strict.len());
        assert_eq!(bytes_written, bytes_written_precise_fit);
        assert_eq!(&buf[0..(params.total_len)], &buf_strict[..]);

        // since we have a copy of the wrapped bytes lying around, remove the endings and see
        // if we get the original
        for line_ending_num in 0..params.lines_with_endings {
            let line_ending_offset = (line_ending_num + 1) * line_len;

            for _ in 0..line_ending.len() {
                let _ = buf_strict.remove(line_ending_offset);
            }
        }
        assert_eq!(orig_buf, buf_strict);
    }

    buf.truncate(params.total_len);

    bytes_written
}