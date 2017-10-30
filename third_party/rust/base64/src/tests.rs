extern crate rand;

use super::*;
use super::line_wrap::{line_wrap_parameters};
use self::rand::Rng;
use self::rand::distributions::{IndependentSample, Range};

#[test]
fn encoded_size_correct_standard() {
    assert_encoded_length(0, 0, STANDARD);

    assert_encoded_length(1, 4, STANDARD);
    assert_encoded_length(2, 4, STANDARD);
    assert_encoded_length(3, 4, STANDARD);

    assert_encoded_length(4, 8, STANDARD);
    assert_encoded_length(5, 8, STANDARD);
    assert_encoded_length(6, 8, STANDARD);

    assert_encoded_length(7, 12, STANDARD);
    assert_encoded_length(8, 12, STANDARD);
    assert_encoded_length(9, 12, STANDARD);

    assert_encoded_length(54, 72, STANDARD);

    assert_encoded_length(55, 76, STANDARD);
    assert_encoded_length(56, 76, STANDARD);
    assert_encoded_length(57, 76, STANDARD);

    assert_encoded_length(58, 80, STANDARD);
}

#[test]
fn encoded_size_correct_no_pad_no_wrap() {
    assert_encoded_length(0, 0, URL_SAFE_NO_PAD);

    assert_encoded_length(1, 2, URL_SAFE_NO_PAD);
    assert_encoded_length(2, 3, URL_SAFE_NO_PAD);
    assert_encoded_length(3, 4, URL_SAFE_NO_PAD);

    assert_encoded_length(4, 6, URL_SAFE_NO_PAD);
    assert_encoded_length(5, 7, URL_SAFE_NO_PAD);
    assert_encoded_length(6, 8, URL_SAFE_NO_PAD);

    assert_encoded_length(7, 10, URL_SAFE_NO_PAD);
    assert_encoded_length(8, 11, URL_SAFE_NO_PAD);
    assert_encoded_length(9, 12, URL_SAFE_NO_PAD);

    assert_encoded_length(54, 72, URL_SAFE_NO_PAD);

    assert_encoded_length(55, 74, URL_SAFE_NO_PAD);
    assert_encoded_length(56, 75, URL_SAFE_NO_PAD);
    assert_encoded_length(57, 76, URL_SAFE_NO_PAD);

    assert_encoded_length(58, 78, URL_SAFE_NO_PAD);
}

#[test]
fn encoded_size_correct_mime() {
    assert_encoded_length(0, 0, MIME);

    assert_encoded_length(1, 4, MIME);
    assert_encoded_length(2, 4, MIME);
    assert_encoded_length(3, 4, MIME);

    assert_encoded_length(4, 8, MIME);
    assert_encoded_length(5, 8, MIME);
    assert_encoded_length(6, 8, MIME);

    assert_encoded_length(7, 12, MIME);
    assert_encoded_length(8, 12, MIME);
    assert_encoded_length(9, 12, MIME);

    assert_encoded_length(54, 72, MIME);

    assert_encoded_length(55, 76, MIME);
    assert_encoded_length(56, 76, MIME);
    assert_encoded_length(57, 76, MIME);

    assert_encoded_length(58, 82, MIME);
    assert_encoded_length(59, 82, MIME);
    assert_encoded_length(60, 82, MIME);
}

#[test]
fn encoded_size_correct_lf_pad() {
    let config = Config::new(
        CharacterSet::Standard,
        true,
        false,
        LineWrap::Wrap(76, LineEnding::LF)
    );

    assert_encoded_length(0, 0, config);

    assert_encoded_length(1, 4, config);
    assert_encoded_length(2, 4, config);
    assert_encoded_length(3, 4, config);

    assert_encoded_length(4, 8, config);
    assert_encoded_length(5, 8, config);
    assert_encoded_length(6, 8, config);

    assert_encoded_length(7, 12, config);
    assert_encoded_length(8, 12, config);
    assert_encoded_length(9, 12, config);

    assert_encoded_length(54, 72, config);

    assert_encoded_length(55, 76, config);
    assert_encoded_length(56, 76, config);
    assert_encoded_length(57, 76, config);

    // one fewer than MIME
    assert_encoded_length(58, 81, config);
    assert_encoded_length(59, 81, config);
    assert_encoded_length(60, 81, config);
}

#[test]
fn encoded_size_overflow() {
    assert_eq!(None, encoded_size(std::usize::MAX, &STANDARD));
}

#[test]
fn roundtrip_random_config_short() {
    // exercise the slower encode/decode routines that operate on shorter buffers more vigorously
    roundtrip_random_config(Range::new(0, 50), Range::new(0, 50), 10_000);
}

#[test]
fn roundtrip_random_config_long() {
    roundtrip_random_config(Range::new(0, 1000), Range::new(0, 1000), 10_000);
}

#[test]
fn encode_into_nonempty_buffer_doesnt_clobber_existing_contents() {
    let mut orig_data = Vec::new();
    let mut prefix = String::new();
    let mut encoded_data_no_prefix = String::new();
    let mut encoded_data_with_prefix = String::new();
    let mut decoded = Vec::new();

    let prefix_len_range = Range::new(0, 1000);
    let input_len_range = Range::new(0, 1000);
    let line_len_range = Range::new(1, 1000);

    let mut rng = rand::weak_rng();

    for _ in 0..10_000 {
        orig_data.clear();
        prefix.clear();
        encoded_data_no_prefix.clear();
        encoded_data_with_prefix.clear();
        decoded.clear();

        let input_len = input_len_range.ind_sample(&mut rng);

        for _ in 0..input_len {
            orig_data.push(rng.gen());
        }

        let prefix_len = prefix_len_range.ind_sample(&mut rng);
        for _ in 0..prefix_len {
            // getting convenient random single-byte printable chars that aren't base64 is annoying
            prefix.push('#');
        }
        encoded_data_with_prefix.push_str(&prefix);

        let config = random_config(&mut rng, &line_len_range);
        encode_config_buf(&orig_data, config, &mut encoded_data_no_prefix);
        encode_config_buf(&orig_data, config, &mut encoded_data_with_prefix);

        assert_eq!(encoded_data_no_prefix.len() + prefix_len, encoded_data_with_prefix.len());
        assert_encode_sanity(&encoded_data_no_prefix, &config, input_len);
        assert_encode_sanity(&encoded_data_with_prefix[prefix_len..], &config, input_len);

        // append plain encode onto prefix
        prefix.push_str(&mut encoded_data_no_prefix);

        assert_eq!(prefix, encoded_data_with_prefix);

        // since we know we have the correct count of line endings, it's reasonable to simply remove
        // them without worrying about where they are
        let encoded_no_line_endings: String = encoded_data_no_prefix.chars()
            .filter(|&c| c != '\r' && c != '\n')
            .collect();

        decode_config_buf(&encoded_no_line_endings, config, &mut decoded).unwrap();
        assert_eq!(orig_data, decoded);
    }
}

#[test]
fn decode_into_nonempty_buffer_doesnt_clobber_existing_contents() {
    let mut orig_data = Vec::new();
    let mut encoded_data = String::new();
    let mut decoded_with_prefix = Vec::new();
    let mut decoded_without_prefix = Vec::new();
    let mut prefix = Vec::new();

    let prefix_len_range = Range::new(0, 1000);
    let input_len_range = Range::new(0, 1000);
    let line_len_range = Range::new(1, 1000);

    let mut rng = rand::weak_rng();

    for _ in 0..10_000 {
        orig_data.clear();
        encoded_data.clear();
        decoded_with_prefix.clear();
        decoded_without_prefix.clear();
        prefix.clear();

        let input_len = input_len_range.ind_sample(&mut rng);

        for _ in 0..input_len {
            orig_data.push(rng.gen());
        }

        let config = random_config(&mut rng, &line_len_range);
        encode_config_buf(&orig_data, config, &mut encoded_data);
        assert_encode_sanity(&encoded_data, &config, input_len);

        let prefix_len = prefix_len_range.ind_sample(&mut rng);

        // fill the buf with a prefix
        for _ in 0..prefix_len {
            prefix.push(rng.gen());
        }

        decoded_with_prefix.resize(prefix_len, 0);
        decoded_with_prefix.copy_from_slice(&prefix);

        // remove line wrapping
        let encoded_no_line_endings: String = encoded_data.chars()
            .filter(|&c| c != '\r' && c != '\n')
            .collect();

        // decode into the non-empty buf
        decode_config_buf(&encoded_no_line_endings, config, &mut decoded_with_prefix).unwrap();
        // also decode into the empty buf
        decode_config_buf(&encoded_no_line_endings, config, &mut decoded_without_prefix).unwrap();

        assert_eq!(prefix_len + decoded_without_prefix.len(), decoded_with_prefix.len());
        assert_eq!(orig_data, decoded_without_prefix);

        // append plain decode onto prefix
        prefix.append(&mut decoded_without_prefix);

        assert_eq!(prefix, decoded_with_prefix);
    }
}

#[test]
fn encode_with_padding_random_valid_utf8() {
    let mut input = Vec::new();
    let mut output = Vec::new();

    let input_len_range = Range::new(0, 1000);
    let line_len_range = Range::new(1, 1000);

    let mut rng = rand::weak_rng();

    for _ in 0..10_000 {
        input.clear();
        output.clear();

        let input_len = input_len_range.ind_sample(&mut rng);

        for _ in 0..input_len {
            input.push(rng.gen());
        }

        let config = random_config(&mut rng, &line_len_range);

        // fill up the output buffer with garbage
        let encoded_size = encoded_size(input_len, &config).unwrap();
        for _ in 0..encoded_size {
            output.push(rng.gen());
        }

        let orig_output_buf = output.to_vec();

        let bytes_written =
            encode_with_padding(&input, &mut output, config.char_set.encode_table(), config.pad);

        let line_ending_bytes = total_line_ending_bytes(bytes_written, &config);
        assert_eq!(encoded_size, bytes_written + line_ending_bytes);
        // make sure the part beyond bytes_written is the same garbage it was before
        assert_eq!(orig_output_buf[bytes_written..], output[bytes_written..]);

        // make sure the encoded bytes are UTF-8
        let _ = str::from_utf8(&output[0..bytes_written]).unwrap();
    }
}

#[test]
fn encode_to_slice_random_valid_utf8() {
    let mut input = Vec::new();
    let mut output = Vec::new();

    let input_len_range = Range::new(0, 1000);
    let line_len_range = Range::new(1, 1000);

    let mut rng = rand::weak_rng();

    for _ in 0..10_000 {
        input.clear();
        output.clear();

        let input_len = input_len_range.ind_sample(&mut rng);

        for _ in 0..input_len {
            input.push(rng.gen());
        }

        let config = random_config(&mut rng, &line_len_range);


        // fill up the output buffer with garbage
        let encoded_size = encoded_size(input_len, &config).unwrap();
        for _ in 0..encoded_size {
            output.push(rng.gen());
        }

        let orig_output_buf = output.to_vec();

        let bytes_written =
            encode_to_slice(&input, &mut output, config.char_set.encode_table());

        // make sure the part beyond bytes_written is the same garbage it was before
        assert_eq!(orig_output_buf[bytes_written..], output[bytes_written..]);

        // make sure the encoded bytes are UTF-8
        let _ = str::from_utf8(&output[0..bytes_written]).unwrap();
    }
}

#[test]
fn add_padding_random_valid_utf8(){
    let mut output = Vec::new();

    let mut rng = rand::weak_rng();

    // cover our bases for length % 3
    for input_len in 0..10 {
        output.clear();

        // fill output with random
        for _ in 0..10 {
            output.push(rng.gen());
        }

        let orig_output_buf = output.to_vec();

        let bytes_written =
            add_padding(input_len, &mut output);

        // make sure the part beyond bytes_written is the same garbage it was before
        assert_eq!(orig_output_buf[bytes_written..], output[bytes_written..]);

        // make sure the encoded bytes are UTF-8
        let _ = str::from_utf8(&output[0..bytes_written]).unwrap();
    }
}

fn total_line_ending_bytes(encoded_len: usize, config: &Config) -> usize {
    match config.line_wrap {
        LineWrap::NoWrap => 0,
        LineWrap::Wrap(line_len, line_ending) =>
            line_wrap_parameters(encoded_len, line_len, line_ending).total_line_endings_len
    }
}

fn assert_encoded_length(input_len: usize, encoded_len: usize, config: Config) {
    assert_eq!(encoded_len, encoded_size(input_len, &config).unwrap());

    let mut bytes: Vec<u8> = Vec::new();
    let mut rng = rand::weak_rng();

    for _ in 0..input_len {
        bytes.push(rng.gen());
    }

    let encoded = encode_config(&bytes, config);
    assert_encode_sanity(&encoded, &config, input_len);

    assert_eq!(encoded_len, encoded.len());
}

fn assert_encode_sanity(encoded: &str, config: &Config, input_len: usize) {
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
        LineWrap::Wrap(line_len, line_ending) =>
            line_wrap_parameters(b64_only_len, line_len, line_ending).total_line_endings_len
    };

    let expected_encoded_len = encoded_size(input_len, &config).unwrap();

    assert_eq!(expected_encoded_len, encoded.len());

    let line_endings_len = encoded.chars().filter(|&c| c == '\r' || c == '\n').count();
    let padding_len = encoded.chars().filter(|&c| c == '=').count();

    assert_eq!(expected_padding_len, padding_len);
    assert_eq!(expected_line_ending_len, line_endings_len);

    let _ = str::from_utf8(encoded.as_bytes()).expect("Base64 should be valid utf8");
}

fn roundtrip_random_config(input_len_range: Range<usize>, line_len_range: Range<usize>,
                           iterations: u32) {
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

        // remove line wrapping
        let encoded_no_line_endings: String = encoded_buf.chars()
            .filter(|&c| c != '\r' && c != '\n')
            .collect();

        assert_eq!(input_buf, decode_config(&encoded_no_line_endings, config).unwrap());
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

    let charset = if rng.gen() {
        CharacterSet::UrlSafe
    } else {
        CharacterSet::Standard
    };

    Config::new(charset, rng.gen(), rng.gen(), line_wrap)
}
