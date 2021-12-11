extern crate plist;

use plist::{Error, Value};
use std::io::Cursor;

#[test]
fn too_large_allocation() {
    let data = b"bplist00\"&L^^^^^^^^-^^^^^^^^^^^";
    test_fuzzer_data_err(data);
}

#[test]
fn too_large_allocation_2() {
    let data = b"bplist00;<)\x9fX\x0a<h\x0a:hhhhG:hh\x0amhhhhhhx#hhT)\x0a*";
    test_fuzzer_data_err(data);
}

#[test]
fn empty_offset_table() {
    let data = b"bplist00;\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00<)\x9fXTX(";
    test_fuzzer_data_err(data);
}

#[test]
fn binary_circular_reference() {
    let data = b"bplist00\xd6\x01\x02\x03\x04\x05\x06\x07\x0a\x0b\x0c\x0d\x0eULinesUDeathVHeightYBirthdateVAutbplist00\xd6\x01\x02\x03\x04\x05\x06\x07\x0a\x0b\x0c\x0d\x0eULinesUDeathVHeightYBirthdateVAuthorTData\xa2\x08\x09_\x10\x1eIt is nifying nothing.\x11\x06\x1c#?\xf9\x99\x99\x99\x99\x99\x9a3\xc1\xc2v\x00e\x00\x00\x00_\x10\x13William ShakespeareO\x10\x0f\x00\x00\x00\xbe\x00\x00\x00\x03\x00\x00\x00\x1e\x00\x00\x00\x08\x15\x1b!(58>Ab\x90\x93\x9c\xa5\xbb\x00\x00\x00\x00\x00\x00\x01\x01\x00\x00\x00\x00\x00\x00\x00\x0f\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xcd";
    test_fuzzer_data_err(data);
}

#[test]
fn binary_zero_offset_size() {
    let data = include_bytes!("data/binary_zero_offset_size.plist");
    test_fuzzer_data_err(data);
}

#[test]
fn binary_nan_date() {
    let data = b"bplist00\xd6\x01\x02\x01\x04\x05\x06\x07\x0a\x0b\x0c\x0d\x0eULinesUDeathVHeightYBthridateVAuthorTData\xa2\x08\x09_\x10\x1eIt is a tale told by an idiot,_\x10+Full of sound and fury, signifying nothing.\x11\x06\x1c#?\xf9\x99\x99\x99\x99\x99\x9a3\xff\xff\xff\xffe\x00\x00\x00_\x13\x10William ShakespeareO\x10\xe5\x00\x00\x00\xbe\x00\x00\x00\x03\x00\x00\x00\x1e\x00\x00\x00\x08\x15\x1b!(14>Ab\x90\x93\x9c\xa5\xbb\xd4\x00\x00\x00\x00\x00\x01\x01\x00\x00\x00\x00\x00\x00\x00\x0f\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xcd";
    test_fuzzer_data_err(data);
}

#[test]
fn binary_circular_array() {
    let data = include_bytes!("data/binary_circular_array.plist");
    test_fuzzer_data_err(data);
}

// Issue 20 - not found by fuzzing but this is a convenient place to put the test.
#[test]
fn issue_20_binary_with_data_in_trailer() {
    let data =
        b"bplist00\xd0\x08\0\0\0\0\0\0\x01\x01\0\0\0\0\0\0\0\x01\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\t";
    test_fuzzer_data_ok(data);
}

#[test]
fn issue_22_binary_with_byte_ref_size() {
    let data = b"bplist00\xd1\x01\x02TTestQ1\x08\x0b\x10\x00\x00\x00\x00\x00\x00\x01\x01\x00\x00\x00\x00\x00\x00\x00\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x12";
    test_fuzzer_data_ok(data);
}

fn test_fuzzer_data(data: &[u8]) -> Result<Value, Error> {
    let cursor = Cursor::new(data);
    Value::from_reader(cursor)
}

fn test_fuzzer_data_ok(data: &[u8]) {
    test_fuzzer_data(data).unwrap();
}

fn test_fuzzer_data_err(data: &[u8]) {
    assert!(test_fuzzer_data(data).is_err());
}
