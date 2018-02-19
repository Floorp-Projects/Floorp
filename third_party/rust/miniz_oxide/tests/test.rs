extern crate miniz_oxide;

use std::io::Read;

use miniz_oxide::inflate::{TINFLStatus, decompress_to_vec_zlib, decompress_to_vec};
use miniz_oxide::deflate::compress_to_vec;

fn get_test_file_data(name: &str) -> Vec<u8> {
    use std::fs::File;
    let mut input = Vec::new();
    let mut f = File::open(name).unwrap();

    f.read_to_end(&mut input).unwrap();
    input
}

/// Fuzzed file that caused issues for the inflate library.
#[test]
fn inf_issue_14() {
    let data = get_test_file_data("tests/test_data/issue_14.zlib");
    let result = decompress_to_vec_zlib(data.as_slice());
    assert!(result.is_err());
    let error = result.unwrap_err();
    assert_eq!(error, TINFLStatus::Failed);
}

fn get_test_data() -> Vec<u8> {
    use std::env;
    let path = env::var("TEST_FILE").unwrap_or_else(|_| "../miniz/miniz.c".to_string());
    get_test_file_data(&path)
}

fn roundtrip(level: u8) {
    let data = get_test_data();
    let enc = compress_to_vec(&data.as_slice()[..], level);
    println!(
        "Input len: {}, compressed len: {}, level: {}",
        data.len(),
        enc.len(),
        level
    );
    let dec = decompress_to_vec(enc.as_slice()).unwrap();
    assert!(data == dec);
}

#[test]
fn roundtrip_lvl_9() {
    roundtrip(9);
}

#[test]
fn roundtrip_lvl_1() {
    roundtrip(1);
}

#[test]
fn roundtrip_lvl_0() {
    roundtrip(0);
}
