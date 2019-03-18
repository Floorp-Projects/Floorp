extern crate base64;

use base64::*;

fn compare_encode(expected: &str, target: &[u8]) {
    assert_eq!(expected, encode(target));
}

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

    compare_encode(
        "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7P\
         D0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn8\
         =",
        &ascii,
    );
}

#[test]
fn encode_all_bytes() {
    let mut bytes = Vec::<u8>::with_capacity(256);

    for i in 0..255 {
        bytes.push(i);
    }
    bytes.push(255); //bug with "overflowing" ranges?

    compare_encode(
        "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7P\
         D0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn\
         +AgYKDhIWGh4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmqq6ytrq+wsbKztLW2t7i5uru8vb6\
         /wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/g4eLj5OXm5+jp6uvs7e7v8PHy8/T19vf4+fr7/P3+/w==",
        &bytes,
    );
}

#[test]
fn encode_all_bytes_url() {
    let mut bytes = Vec::<u8>::with_capacity(256);

    for i in 0..255 {
        bytes.push(i);
    }
    bytes.push(255); //bug with "overflowing" ranges?

    assert_eq!(
        "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0\
         -P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn\
         -AgYKDhIWGh4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmqq6ytrq\
         -wsbKztLW2t7i5uru8vb6_wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t_g4eLj5OXm5-jp6uvs7e7v8PHy\
         8_T19vf4-fr7_P3-_w==",
        encode_config(&bytes, URL_SAFE)
    );
}

#[test]
fn encode_line_ending_lf_partial_last_line() {
    let config = Config::new(
        CharacterSet::Standard,
        true,
        false,
        LineWrap::Wrap(3, LineEnding::LF),
    );
    assert_eq!("Zm9\nvYm\nFy", encode_config(b"foobar", config));
}

#[test]
fn encode_line_ending_crlf_partial_last_line() {
    let config = Config::new(
        CharacterSet::Standard,
        true,
        false,
        LineWrap::Wrap(3, LineEnding::CRLF),
    );
    assert_eq!("Zm9\r\nvYm\r\nFy", encode_config(b"foobar", config));
}

#[test]
fn encode_line_ending_lf_full_last_line() {
    let config = Config::new(
        CharacterSet::Standard,
        true,
        false,
        LineWrap::Wrap(4, LineEnding::LF),
    );
    assert_eq!("Zm9v\nYmFy", encode_config(b"foobar", config));
}

#[test]
fn encode_line_ending_crlf_full_last_line() {
    let config = Config::new(
        CharacterSet::Standard,
        true,
        false,
        LineWrap::Wrap(4, LineEnding::CRLF),
    );
    assert_eq!("Zm9v\r\nYmFy", encode_config(b"foobar", config));
}

#[test]
fn encode_url_safe_without_padding() {
    let encoded = encode_config(b"alice", URL_SAFE_NO_PAD);
    assert_eq!(&encoded, "YWxpY2U");
    assert_eq!(
        String::from_utf8(decode(&encoded).unwrap()).unwrap(),
        "alice"
    );
}
