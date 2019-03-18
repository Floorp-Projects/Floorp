extern crate rand;

use encode::encoded_size;
use line_wrap::line_wrap_parameters;
use *;

use std::str;

use self::rand::distributions::{IndependentSample, Range};
use self::rand::Rng;

#[test]
fn roundtrip_random_config_short() {
    // exercise the slower encode/decode routines that operate on shorter buffers more vigorously
    roundtrip_random_config(Range::new(0, 50), Range::new(0, 50), 10_000);
}

#[test]
fn roundtrip_random_config_long() {
    roundtrip_random_config(Range::new(0, 1000), Range::new(0, 1000), 10_000);
}

pub fn assert_encode_sanity(encoded: &str, config: &Config, input_len: usize) {
    let input_rem = input_len % 3;
    let (expected_padding_len, last_encoded_chunk_len) = if input_rem > 0 {
        if config.pad {
            (3 - input_rem, 4)
        } else {
            (0, input_rem + 1)
        }
    } else {
        (0, 0)
    };

    let b64_only_len = (input_len / 3) * 4 + last_encoded_chunk_len;

    let expected_line_ending_len = match config.line_wrap {
        LineWrap::NoWrap => 0,
        LineWrap::Wrap(line_len, line_ending) => {
            line_wrap_parameters(b64_only_len, line_len, line_ending).total_line_endings_len
        }
    };

    let expected_encoded_len = encoded_size(input_len, &config).unwrap();

    assert_eq!(expected_encoded_len, encoded.len());

    let line_endings_len = encoded.chars().filter(|&c| c == '\r' || c == '\n').count();
    let padding_len = encoded.chars().filter(|&c| c == '=').count();

    assert_eq!(expected_padding_len, padding_len);
    assert_eq!(expected_line_ending_len, line_endings_len);

    let _ = str::from_utf8(encoded.as_bytes()).expect("Base64 should be valid utf8");
}

fn roundtrip_random_config(
    input_len_range: Range<usize>,
    line_len_range: Range<usize>,
    iterations: u32,
) {
    let mut input_buf: Vec<u8> = Vec::new();
    let mut encoded_buf = String::new();
    let mut rng = rand::weak_rng();

    for _ in 0..iterations {
        input_buf.clear();
        encoded_buf.clear();

        let input_len = input_len_range.ind_sample(&mut rng);

        let config = random_config(&mut rng, &line_len_range);

        for _ in 0..input_len {
            input_buf.push(rng.gen());
        }

        encode_config_buf(&input_buf, config, &mut encoded_buf);

        assert_encode_sanity(&encoded_buf, &config, input_len);

        assert_eq!(input_buf, decode_config(&encoded_buf, config).unwrap());
    }
}

pub fn random_config<R: Rng>(rng: &mut R, line_len_range: &Range<usize>) -> Config {
    let line_wrap = if rng.gen() {
        LineWrap::NoWrap
    } else {
        let line_len = line_len_range.ind_sample(rng);

        let line_ending = if rng.gen() {
            LineEnding::LF
        } else {
            LineEnding::CRLF
        };

        LineWrap::Wrap(line_len, line_ending)
    };

    const CHARSETS: &[CharacterSet] = &[
        CharacterSet::UrlSafe,
        CharacterSet::Standard,
        CharacterSet::Crypt,
    ];
    let charset = *rng.choose(CHARSETS).unwrap();

    let strip_whitespace = match line_wrap {
        LineWrap::NoWrap => false,
        _ => true,
    };

    Config::new(charset, rng.gen(), strip_whitespace, line_wrap)
}
