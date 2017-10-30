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

fn push_rand(buf: &mut Vec<u8>, len: usize) {
    let mut r = rand::weak_rng();

    for _ in 0..len {
        buf.push(r.gen::<u8>());
    }
}

// generate every possible byte string recursively and test encode/decode roundtrip
fn roundtrip_append_recurse(byte_buf: &mut Vec<u8>, str_buf: &mut String, remaining_bytes: usize) {
    let orig_length = byte_buf.len();
    for b in 0..256 {
        byte_buf.push(b as u8);

        if remaining_bytes > 1 {
            roundtrip_append_recurse(byte_buf, str_buf, remaining_bytes - 1)
        } else {
            encode_config_buf(&byte_buf, STANDARD, str_buf);
            let roundtrip_bytes = decode_config(&str_buf, STANDARD).unwrap();
            assert_eq!(*byte_buf, roundtrip_bytes);

            str_buf.clear();

        }

        byte_buf.truncate(orig_length);
    }
}

// generate every possible byte string recursively and test encode/decode roundtrip with
// padding removed
fn roundtrip_append_recurse_strip_padding(byte_buf: &mut Vec<u8>, str_buf: &mut String,
                                          remaining_bytes: usize) {
    let orig_length = byte_buf.len();
    for b in 0..256 {
        byte_buf.push(b as u8);

        if remaining_bytes > 1 {
            roundtrip_append_recurse_strip_padding(byte_buf, str_buf, remaining_bytes - 1)
        } else {
            encode_config_buf(&byte_buf, STANDARD, str_buf);
            {
                let trimmed = str_buf.trim_right_matches('=');
                let roundtrip_bytes = decode_config(&trimmed, STANDARD).unwrap();
                assert_eq!(*byte_buf, roundtrip_bytes);
            }
            str_buf.clear();
        }

        byte_buf.truncate(orig_length);
    }
}

// generate random contents of the specified length and test encode/decode roundtrip
fn roundtrip_random(byte_buf: &mut Vec<u8>, str_buf: &mut String, byte_len: usize,
                    approx_values_per_byte: u8, max_rounds: u64) {
    let num_rounds = calculate_number_of_rounds(byte_len, approx_values_per_byte, max_rounds);
    let mut r = rand::weak_rng();

    for _ in 0..num_rounds {
        byte_buf.clear();
        str_buf.clear();
        while byte_buf.len() < byte_len {
            byte_buf.push(r.gen::<u8>());
        }

        encode_config_buf(&byte_buf, STANDARD, str_buf);
        let roundtrip_bytes = decode_config(&str_buf, STANDARD).unwrap();

        assert_eq!(*byte_buf, roundtrip_bytes);
    }
}

// generate random contents of the specified length and test encode/decode roundtrip
fn roundtrip_random_strip_padding(byte_buf: &mut Vec<u8>, str_buf: &mut String, byte_len: usize,
                    approx_values_per_byte: u8, max_rounds: u64) {
    // let the short ones be short but don't let it get too crazy large
    let num_rounds = calculate_number_of_rounds(byte_len, approx_values_per_byte, max_rounds);
    let mut r = rand::weak_rng();

    for _ in 0..num_rounds {
        byte_buf.clear();
        str_buf.clear();
        while byte_buf.len() < byte_len {
            byte_buf.push(r.gen::<u8>());
        }

        encode_config_buf(&byte_buf, STANDARD, str_buf);
        let trimmed = str_buf.trim_right_matches('=');
        let roundtrip_bytes = decode_config(&trimmed, STANDARD).unwrap();

        assert_eq!(*byte_buf, roundtrip_bytes);
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

    return prod;
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
    // can't PartialEq Base64Error, so we do this the hard way
    match decode("YWxpY2U=====").unwrap_err() {
        DecodeError::InvalidByte(offset, byte) => {
            // since the first 8 bytes are handled in the fast loop, the
            // padding is an error. Could argue that the *next* padding
            // byte is technically the first erroneous one, but reporting
            // that accurately is more complex and probably nobody cares
            assert_eq!(7, offset);
            assert_eq!(0x3D, byte);
        }
        _ => assert!(false)
    }
}

#[test]
fn decode_extra_pad_after_fast_loop_chunk_error() {
    match decode("YWxpY2UABB===").unwrap_err() {
        DecodeError::InvalidByte(offset, byte) => {
            // extraneous third padding byte
            assert_eq!(12, offset);
            assert_eq!(0x3D, byte);
        }
        _ => assert!(false)
    };
}


//same
#[test]
fn decode_absurd_pad_error() {
    match decode("==Y=Wx===pY=2U=====").unwrap_err() {
        DecodeError::InvalidByte(size, byte) => {
            assert_eq!(0, size);
            assert_eq!(0x3D, byte);
        }
        _ => assert!(false)
    }
}

#[test]
fn decode_starts_with_padding_single_quad_error() {
    match decode("====").unwrap_err() {
        DecodeError::InvalidByte(offset, byte) => {
            // with no real input, first padding byte is bogus
            assert_eq!(0, offset);
            assert_eq!(0x3D, byte);
        }
        _ => assert!(false)
    }
}

#[test]
fn decode_extra_padding_in_trailing_quad_returns_error() {
    match decode("zzz==").unwrap_err() {
        DecodeError::InvalidByte(size, byte) => {
            // first unneeded padding byte
            assert_eq!(4, size);
            assert_eq!(0x3D, byte);
        }
        _ => assert!(false)
    }
}

#[test]
fn decode_extra_padding_in_trailing_quad_2_returns_error() {
    match decode("zz===").unwrap_err() {
        DecodeError::InvalidByte(size, byte) => {
            // first unneeded padding byte
            assert_eq!(4, size);
            assert_eq!(0x3D, byte);
        }
        _ => assert!(false)
    }
}


#[test]
fn decode_start_second_quad_with_padding_returns_error() {
    match decode("zzzz=").unwrap_err() {
        DecodeError::InvalidByte(size, byte) => {
            // first unneeded padding byte
            assert_eq!(4, size);
            assert_eq!(0x3D, byte);
        }
        _ => assert!(false)
    }
}

#[test]
fn decode_padding_in_last_quad_followed_by_non_padding_returns_error() {
    match decode("zzzz==z").unwrap_err() {
        DecodeError::InvalidByte(size, byte) => {
            assert_eq!(4, size);
            assert_eq!(0x3D, byte);
        }
        _ => assert!(false)
    }
}

#[test]
fn decode_too_short_with_padding_error() {
    match decode("z==").unwrap_err() {
        DecodeError::InvalidByte(size, byte) => {
            // first unneeded padding byte
            assert_eq!(1, size);
            assert_eq!(0x3D, byte);
        }
        _ => assert!(false)
    }
}

#[test]
fn decode_too_short_without_padding_error() {
    match decode("z").unwrap_err() {
        DecodeError::InvalidLength => {}
        _ => assert!(false)
    }
}

#[test]
fn decode_too_short_second_quad_without_padding_error() {
    match decode("zzzzX").unwrap_err() {
        DecodeError::InvalidLength => {}
        _ => assert!(false)
    }
}

#[test]
fn decode_error_for_bogus_char_in_right_position() {
    for length in 1..25 {
        for error_position in 0_usize..length {
            let prefix: String = std::iter::repeat("A").take(error_position).collect();
            let suffix: String = std::iter::repeat("B").take(length - error_position - 1).collect();

            let input = prefix + "%" + &suffix;
            assert_eq!(length, input.len(),
                "length {} error position {}", length, error_position);

            match decode(&input).unwrap_err() {
                DecodeError::InvalidByte(size, byte) => {
                    assert_eq!(error_position, size,
                        "length {} error position {}", length, error_position);
                    assert_eq!(0x25, byte);
                }
                _ => assert!(false)
            }
        }
    }
}

#[test]
fn decode_into_nonempty_buffer_doesnt_clobber_existing_contents() {
    let mut orig_data = Vec::new();
    let mut encoded_data = String::new();
    let mut decoded_with_prefix = Vec::new();
    let mut decoded_without_prefix = Vec::new();
    let mut prefix = Vec::new();
    for encoded_length in 0_usize..26 {
        if encoded_length % 4 == 1 {
            // can't have a lone byte in a quad of input
            continue;
        };

        let raw_data_byte_triples = encoded_length / 4;
        // 4 base64 bytes -> 3 input bytes, 3 -> 2, 2 -> 1, 0 -> 0
        let raw_data_byte_leftovers = (encoded_length % 4).saturating_sub(1);

        // we'll borrow buf to make some data to encode
        orig_data.clear();
        push_rand(&mut orig_data, raw_data_byte_triples * 3 + raw_data_byte_leftovers);

        encoded_data.clear();
        encode_config_buf(&orig_data, STANDARD, &mut encoded_data);

        assert_eq!(encoded_length, encoded_data.trim_right_matches('=').len());

        for prefix_length in 1..26 {
            decoded_with_prefix.clear();
            decoded_without_prefix.clear();
            prefix.clear();

            // fill the buf with a prefix
            push_rand(&mut prefix, prefix_length);
            decoded_with_prefix.resize(prefix_length, 0);
            decoded_with_prefix.copy_from_slice(&prefix);

            // decode into the non-empty buf
            decode_config_buf(&encoded_data, STANDARD, &mut decoded_with_prefix).unwrap();
            // also decode into the empty buf
            decode_config_buf(&encoded_data, STANDARD, &mut decoded_without_prefix).unwrap();

            assert_eq!(prefix_length + decoded_without_prefix.len(), decoded_with_prefix.len());

            // append plain decode onto prefix
            prefix.append(&mut decoded_without_prefix);

            assert_eq!(prefix, decoded_with_prefix);
        }
    }
}

#[test]
fn roundtrip_random_no_fast_loop() {
    let mut byte_buf: Vec<u8> = Vec::new();
    let mut str_buf = String::new();

    for input_len in 0..9 {
        roundtrip_random(&mut byte_buf, &mut str_buf, input_len, 4, 10000);
    }
}

#[test]
fn roundtrip_random_with_fast_loop() {
    let mut byte_buf: Vec<u8> = Vec::new();
    let mut str_buf = String::new();

    for input_len in 9..26 {
        roundtrip_random(&mut byte_buf, &mut str_buf, input_len, 4, 100000);
    }
}

#[test]
fn roundtrip_random_no_fast_loop_no_padding() {
    let mut byte_buf: Vec<u8> = Vec::new();
    let mut str_buf = String::new();

    for input_len in 0..9 {
        roundtrip_random_strip_padding(&mut byte_buf, &mut str_buf, input_len, 4, 10000);
    }
}

#[test]
fn roundtrip_random_with_fast_loop_no_padding() {
    let mut byte_buf: Vec<u8> = Vec::new();
    let mut str_buf = String::new();

    for input_len in 9..26 {
        roundtrip_random_strip_padding(&mut byte_buf, &mut str_buf, input_len, 4, 100000);
    }
}

#[test]
fn roundtrip_all_1_byte() {
    let mut byte_buf: Vec<u8> = Vec::new();
    let mut str_buf = String::new();
    roundtrip_append_recurse(&mut byte_buf, &mut str_buf, 1);
}

#[test]
fn roundtrip_all_1_byte_no_padding() {
    let mut byte_buf: Vec<u8> = Vec::new();
    let mut str_buf = String::new();
    roundtrip_append_recurse_strip_padding(&mut byte_buf, &mut str_buf, 1);
}

#[test]
fn roundtrip_all_2_byte() {
    let mut byte_buf: Vec<u8> = Vec::new();
    let mut str_buf = String::new();
    roundtrip_append_recurse(&mut byte_buf, &mut str_buf, 2);
}

#[test]
fn roundtrip_all_2_byte_no_padding() {
    let mut byte_buf: Vec<u8> = Vec::new();
    let mut str_buf = String::new();
    roundtrip_append_recurse_strip_padding(&mut byte_buf, &mut str_buf, 2);
}

#[test]
fn roundtrip_all_3_byte() {
    let mut byte_buf: Vec<u8> = Vec::new();
    let mut str_buf = String::new();
    roundtrip_append_recurse(&mut byte_buf, &mut str_buf, 3);
}

#[test]
fn roundtrip_random_4_byte() {
    let mut byte_buf: Vec<u8> = Vec::new();
    let mut str_buf = String::new();

    roundtrip_random(&mut byte_buf, &mut str_buf, 4, 48, 10000);
}

//TODO like, write a thing to test every ascii val lol
//prolly just yankput the 64 array and a 256 one later
//is there a way to like, not have to write a fn every time
//"hi test harness this should panic 192 times" would be nice
//oh well whatever this is better done by a fuzzer

//strip yr whitespace kids
#[test]
#[should_panic]
fn decode_reject_space() {
    assert!(decode("YWx pY2U=").is_ok());
}

#[test]
#[should_panic]
fn decode_reject_tab() {
    assert!(decode("YWx\tpY2U=").is_ok());
}

#[test]
#[should_panic]
fn decode_reject_ff() {
    assert!(decode("YWx\x0cpY2U=").is_ok());
}

#[test]
#[should_panic]
fn decode_reject_vtab() {
    assert!(decode("YWx\x0bpY2U=").is_ok());
}

#[test]
#[should_panic]
fn decode_reject_nl() {
    assert!(decode("YWx\npY2U=").is_ok());
}

#[test]
#[should_panic]
fn decode_reject_crnl() {
    assert!(decode("YWx\r\npY2U=").is_ok());
}

#[test]
#[should_panic]
fn decode_reject_null() {
    assert!(decode("YWx\0pY2U=").is_ok());
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
#[should_panic]
fn decode_mime_reject_null() {
    assert!(decode_config("YWx\0pY2U=", MIME).is_ok());
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
fn encode_into_nonempty_buffer_doesnt_clobber_existing_contents() {
    let mut orig_data = Vec::new();
    let mut encoded_with_prefix = String::new();
    let mut encoded_without_prefix = String::new();
    let mut prefix = String::new();
    for orig_data_length in 0_usize..26 {
        // we'll borrow buf to make some data to encode
        orig_data.clear();
        push_rand(&mut orig_data, orig_data_length);

        for prefix_length in 1..26 {
            encoded_with_prefix.clear();
            encoded_without_prefix.clear();
            prefix.clear();

            for _ in 0..prefix_length {
                prefix.push('~');
            }

            encoded_with_prefix.push_str(&prefix);

            // encode into the non-empty buf
            encode_config_buf(&orig_data, STANDARD, &mut encoded_with_prefix);
            // also encode into the empty buf
            encode_config_buf(&orig_data, STANDARD, &mut encoded_without_prefix);

            assert_eq!(prefix_length + encoded_without_prefix.len(), encoded_with_prefix.len());

            // append plain decode onto prefix
            prefix.push_str(&mut encoded_without_prefix);

            assert_eq!(prefix, encoded_with_prefix);
        }
    }
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
