extern crate base64;
extern crate rand;

use rand::Rng;

use base64::*;

fn compare_encode(expected: &str, target: &[u8]) {
    assert_eq!(expected, encode(target));
}

fn compare_decode(expected: &str, target: &str) {
    assert_eq!(expected, String::from_utf8(decode(target).unwrap()).unwrap());
    assert_eq!(expected, String::from_utf8(decode(target.as_bytes()).unwrap()).unwrap());
}

fn compare_decode_mime(expected: &str, target: &str) {
    assert_eq!(expected, String::from_utf8(decode_config(target, MIME).unwrap()).unwrap());
}

// generate random contents of the specified length and test encode/decode roundtrip
fn roundtrip_random(byte_buf: &mut Vec<u8>, str_buf: &mut String, config: Config,
                    byte_len: usize, approx_values_per_byte: u8, max_rounds: u64) {
    // let the short ones be short but don't let it get too crazy large
    let num_rounds = calculate_number_of_rounds(byte_len, approx_values_per_byte, max_rounds);
    let mut r = rand::weak_rng();
    let mut decode_buf = Vec::new();

    for _ in 0..num_rounds {
        byte_buf.clear();
        str_buf.clear();
        decode_buf.clear();
        while byte_buf.len() < byte_len {
            byte_buf.push(r.gen::<u8>());
        }

        encode_config_buf(&byte_buf, config, str_buf);
        decode_config_buf(&str_buf, config, &mut decode_buf).unwrap();

        assert_eq!(byte_buf, &decode_buf);
    }
}

fn calculate_number_of_rounds(byte_len: usize, approx_values_per_byte: u8, max: u64) -> u64 {
    // don't overflow
    let mut prod = approx_values_per_byte as u64;

    for _ in 0..byte_len {
        if prod > max {
            return max;
        }

        prod = prod.saturating_mul(prod);
    }

    prod
}

fn no_pad_config() -> Config {
    Config::new(CharacterSet::Standard, false, false, LineWrap::NoWrap)
}

//-------
//decode

#[test]
fn decode_rfc4648_0() {
    compare_decode("", "");
}

#[test]
fn decode_rfc4648_1() {
    compare_decode("f", "Zg==");
}

#[test]
fn decode_rfc4648_1_just_a_bit_of_padding() {
    // allows less padding than required
    compare_decode("f", "Zg=");
}

#[test]
fn decode_rfc4648_1_no_padding() {
    compare_decode("f", "Zg");
}

#[test]
fn decode_rfc4648_2() {
    compare_decode("fo", "Zm8=");
}

#[test]
fn decode_rfc4648_2_no_padding() {
    compare_decode("fo", "Zm8");
}

#[test]
fn decode_rfc4648_3() {
    compare_decode("foo", "Zm9v");
}

#[test]
fn decode_rfc4648_4() {
    compare_decode("foob", "Zm9vYg==");
}

#[test]
fn decode_rfc4648_4_no_padding() {
    compare_decode("foob", "Zm9vYg");
}

#[test]
fn decode_rfc4648_5() {
    compare_decode("fooba", "Zm9vYmE=");
}

#[test]
fn decode_rfc4648_5_no_padding() {
    compare_decode("fooba", "Zm9vYmE");
}

#[test]
fn decode_rfc4648_6() {
    compare_decode("foobar", "Zm9vYmFy");
}

//this is a MAY in the rfc: https://tools.ietf.org/html/rfc4648#section-3.3
#[test]
fn decode_pad_inside_fast_loop_chunk_error() {
    for num_quads in 0..25 {
        let mut s: String = std::iter::repeat("ABCD").take(num_quads).collect();
        s.push_str("YWxpY2U=====");

        // since the first 8 bytes are handled in the fast loop, the
        // padding is an error. Could argue that the *next* padding
        // byte is technically the first erroneous one, but reporting
        // that accurately is more complex and probably nobody cares
        assert_eq!(DecodeError::InvalidByte(num_quads * 4 + 7, b'='), decode(&s).unwrap_err());
    }
}

#[test]
fn decode_extra_pad_after_fast_loop_chunk_error() {
    for num_quads in 0..25 {
        let mut s: String = std::iter::repeat("ABCD").take(num_quads).collect();
        s.push_str("YWxpY2UABB===");

        // first padding byte
        assert_eq!(DecodeError::InvalidByte(num_quads * 4 + 10, b'='), decode(&s).unwrap_err());
    }
}

#[test]
fn decode_absurd_pad_error() {
    for num_quads in 0..25 {
        let mut s: String = std::iter::repeat("ABCD").take(num_quads).collect();
        s.push_str("==Y=Wx===pY=2U=====");

        // first padding byte
        assert_eq!(DecodeError::InvalidByte(num_quads * 4, b'='), decode(&s).unwrap_err());
    }
}

#[test]
fn decode_extra_padding_in_trailing_quad_returns_error() {
    for num_quads in 0..25 {
        let mut s: String = std::iter::repeat("ABCD").take(num_quads).collect();
        s.push_str("EEE==");

        // first padding byte -- which would be legal if it was by itself
        assert_eq!(DecodeError::InvalidByte(num_quads * 4 + 3, b'='), decode(&s).unwrap_err());
    }
}

#[test]
fn decode_extra_padding_in_trailing_quad_2_returns_error() {
    for num_quads in 0..25 {
        let mut s: String = std::iter::repeat("ABCD").take(num_quads).collect();
        s.push_str("EE===");

        // first padding byte -- which would be legal if it was by itself
        assert_eq!(DecodeError::InvalidByte(num_quads * 4 + 2, b'='), decode(&s).unwrap_err());
    }
}

#[test]
fn decode_start_second_quad_with_padding_returns_error() {
    for num_quads in 0..25 {
        let mut s: String = std::iter::repeat("ABCD").take(num_quads).collect();
        s.push_str("=");

        // first padding byte -- must have two non-padding bytes in a quad
        assert_eq!(DecodeError::InvalidByte(num_quads * 4, b'='), decode(&s).unwrap_err());

        // two padding bytes -- same
        s.push_str("=");
        assert_eq!(DecodeError::InvalidByte(num_quads * 4, b'='), decode(&s).unwrap_err());

        s.push_str("=");
        assert_eq!(DecodeError::InvalidByte(num_quads * 4, b'='), decode(&s).unwrap_err());

        s.push_str("=");
        assert_eq!(DecodeError::InvalidByte(num_quads * 4, b'='), decode(&s).unwrap_err());
    }
}

#[test]
fn decode_padding_in_last_quad_followed_by_non_padding_returns_error() {
    for num_quads in 0..25 {
        let mut s: String = std::iter::repeat("ABCD").take(num_quads).collect();
        s.push_str("==E");

        // first padding byte -- must have two non-padding bytes in a quad
        assert_eq!(DecodeError::InvalidByte(num_quads * 4, b'='), decode(&s).unwrap_err());
    }
}

#[test]
fn decode_one_char_in_quad_with_padding_error() {
    for num_quads in 0..25 {
        let mut s: String = std::iter::repeat("ABCD").take(num_quads).collect();
        s.push_str("E=");

        assert_eq!(DecodeError::InvalidByte(num_quads * 4 + 1, b'='), decode(&s).unwrap_err());

        // more padding doesn't change the error
        s.push_str("=");
        assert_eq!(DecodeError::InvalidByte(num_quads * 4 + 1, b'='), decode(&s).unwrap_err());

        s.push_str("=");
        assert_eq!(DecodeError::InvalidByte(num_quads * 4 + 1, b'='), decode(&s).unwrap_err());
    }
}

#[test]
fn decode_one_char_in_quad_without_padding_error() {
    for num_quads in 0..25 {
        let mut s: String = std::iter::repeat("ABCD").take(num_quads).collect();
        s.push('E');

        assert_eq!(DecodeError::InvalidLength, decode(&s).unwrap_err());
    }
}

#[test]
fn decode_reject_invalid_bytes_with_correct_error() {
    for length in 1..100 {
        for index in 0_usize..length {
            for invalid_byte in " \t\n\r\x0C\x0B\x00%*.".bytes() {
                let prefix: String = std::iter::repeat("A").take(index).collect();
                let suffix: String = std::iter::repeat("B").take(length - index - 1).collect();

                let input = prefix + &String::from_utf8(vec![invalid_byte]).unwrap() + &suffix;
                assert_eq!(length, input.len(), "length {} error position {}", length, index);

                assert_eq!(DecodeError::InvalidByte(index, invalid_byte),
                    decode(&input).unwrap_err());
            }
        }
    }
}

#[test]
fn roundtrip_random_short_standard() {
    let mut byte_buf: Vec<u8> = Vec::new();
    let mut str_buf = String::new();

    for input_len in 0..40 {
        roundtrip_random(&mut byte_buf, &mut str_buf, STANDARD, input_len, 4, 10000);
    }
}

#[test]
fn roundtrip_random_with_fast_loop_standard() {
    let mut byte_buf: Vec<u8> = Vec::new();
    let mut str_buf = String::new();

    for input_len in 40..100 {
        roundtrip_random(&mut byte_buf, &mut str_buf, STANDARD, input_len, 4, 1000);
    }
}

#[test]
fn roundtrip_random_short_no_padding() {
    let mut byte_buf: Vec<u8> = Vec::new();
    let mut str_buf = String::new();

    for input_len in 0..40 {
        roundtrip_random(&mut byte_buf, &mut str_buf, no_pad_config(), input_len, 4, 10000);
    }
}

#[test]
fn roundtrip_random_no_padding() {
    let mut byte_buf: Vec<u8> = Vec::new();
    let mut str_buf = String::new();

    for input_len in 40..100 {
        roundtrip_random(&mut byte_buf, &mut str_buf, no_pad_config(), input_len, 4, 1000);
    }
}

#[test]
fn decode_mime_allow_space() {
    assert!(decode_config("YWx pY2U=", MIME).is_ok());
}

#[test]
fn decode_mime_allow_tab() {
    assert!(decode_config("YWx\tpY2U=", MIME).is_ok());
}

#[test]
fn decode_mime_allow_ff() {
    assert!(decode_config("YWx\x0cpY2U=", MIME).is_ok());
}

#[test]
fn decode_mime_allow_vtab() {
    assert!(decode_config("YWx\x0bpY2U=", MIME).is_ok());
}

#[test]
fn decode_mime_allow_nl() {
    assert!(decode_config("YWx\npY2U=", MIME).is_ok());
}

#[test]
fn decode_mime_allow_crnl() {
    assert!(decode_config("YWx\r\npY2U=", MIME).is_ok());
}

#[test]
fn decode_mime_reject_null() {
    assert_eq!(DecodeError::InvalidByte(3, 0x0),decode_config("YWx\0pY2U=", MIME).unwrap_err());
}

#[test]
fn decode_mime_absurd_whitespace() {
    compare_decode_mime("how could you let this happen",
        "\n aG93I\n\nG\x0bNvd\r\nWxkI HlvdSB \tsZXQgdGh\rpcyBo\x0cYXBwZW4 =   ");
}

//-------
//encode

#[test]
fn encode_rfc4648_0() {
    compare_encode("", b"");
}

#[test]
fn encode_rfc4648_1() {
    compare_encode("Zg==", b"f");
}

#[test]
fn encode_rfc4648_2() {
    compare_encode("Zm8=", b"fo");
}

#[test]
fn encode_rfc4648_3() {
    compare_encode("Zm9v", b"foo");
}

#[test]
fn encode_rfc4648_4() {
    compare_encode("Zm9vYg==", b"foob");
}

#[test]
fn encode_rfc4648_5() {
    compare_encode("Zm9vYmE=", b"fooba");
}

#[test]
fn encode_rfc4648_6() {
    compare_encode("Zm9vYmFy", b"foobar");
}

#[test]
fn encode_all_ascii() {
    let mut ascii = Vec::<u8>::with_capacity(128);

    for i in 0..128 {
        ascii.push(i);
    }

    compare_encode("AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn8=", &ascii);
}

#[test]
fn encode_all_bytes() {
    let mut bytes = Vec::<u8>::with_capacity(256);

    for i in 0..255 {
        bytes.push(i);
    }
    bytes.push(255); //bug with "overflowing" ranges?

    compare_encode("AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn+AgYKDhIWGh4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmqq6ytrq+wsbKztLW2t7i5uru8vb6/wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/g4eLj5OXm5+jp6uvs7e7v8PHy8/T19vf4+fr7/P3+/w==", &bytes);
}

#[test]
fn encode_all_bytes_url() {
    let mut bytes = Vec::<u8>::with_capacity(256);

    for i in 0..255 {
        bytes.push(i);
    }
    bytes.push(255); //bug with "overflowing" ranges?

    assert_eq!("AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0-P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn-AgYKDhIWGh4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmqq6ytrq-wsbKztLW2t7i5uru8vb6_wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t_g4eLj5OXm5-jp6uvs7e7v8PHy8_T19vf4-fr7_P3-_w==", encode_config(&bytes, URL_SAFE));
}

#[test]
fn encode_line_ending_lf_partial_last_line() {
    let config = Config::new(CharacterSet::Standard, true, false,
                             LineWrap::Wrap(3, LineEnding::LF));
    assert_eq!("Zm9\nvYm\nFy", encode_config(b"foobar", config));
}

#[test]
fn encode_line_ending_crlf_partial_last_line() {
    let config = Config::new(CharacterSet::Standard, true, false,
                             LineWrap::Wrap(3, LineEnding::CRLF));
    assert_eq!("Zm9\r\nvYm\r\nFy", encode_config(b"foobar", config));
}

#[test]
fn encode_line_ending_lf_full_last_line() {
    let config = Config::new(CharacterSet::Standard, true, false,
                             LineWrap::Wrap(4, LineEnding::LF));
    assert_eq!("Zm9v\nYmFy", encode_config(b"foobar", config));
}

#[test]
fn encode_line_ending_crlf_full_last_line() {
    let config = Config::new(CharacterSet::Standard, true, false,
                             LineWrap::Wrap(4, LineEnding::CRLF));
    assert_eq!("Zm9v\r\nYmFy", encode_config(b"foobar", config));
}

#[test]
fn because_we_can() {
    compare_decode("alice", "YWxpY2U=");
    compare_decode("alice", &encode(b"alice"));
    compare_decode("alice", &encode(&decode(&encode(b"alice")).unwrap()));
}

#[test]
fn encode_url_safe_without_padding() {
    let encoded = encode_config(b"alice", URL_SAFE_NO_PAD);
    assert_eq!(&encoded, "YWxpY2U");
    assert_eq!(String::from_utf8(decode(&encoded).unwrap()).unwrap(), "alice");
}
