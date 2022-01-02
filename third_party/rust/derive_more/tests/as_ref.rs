#![allow(dead_code)]

#[macro_use]
extern crate derive_more;

use std::path::PathBuf;
use std::ptr;

#[derive(AsRef)]
struct SingleFieldTuple(String);

#[test]
fn single_field_tuple() {
    let item = SingleFieldTuple(String::from("test"));

    assert!(ptr::eq(&item.0, item.as_ref()));
}

#[derive(AsRef)]
#[as_ref(forward)]
struct SingleFieldForward(Vec<i32>);

#[test]
fn single_field_forward() {
    let item = SingleFieldForward(vec![]);
    let _: &[i32] = (&item).as_ref();
}

#[derive(AsRef)]
struct SingleFieldStruct {
    first: String,
}

#[test]
fn single_field_struct() {
    let item = SingleFieldStruct {
        first: String::from("test"),
    };

    assert!(ptr::eq(&item.first, item.as_ref()));
}

#[derive(AsRef)]
struct MultiFieldTuple(#[as_ref] String, #[as_ref] PathBuf, Vec<usize>);

#[test]
fn multi_field_tuple() {
    let item = MultiFieldTuple(String::from("test"), PathBuf::new(), vec![]);

    assert!(ptr::eq(&item.0, item.as_ref()));
    assert!(ptr::eq(&item.1, item.as_ref()));
}

#[derive(AsRef)]
struct MultiFieldStruct {
    #[as_ref]
    first: String,
    #[as_ref]
    second: PathBuf,
    third: Vec<usize>,
}

#[test]
fn multi_field_struct() {
    let item = MultiFieldStruct {
        first: String::from("test"),
        second: PathBuf::new(),
        third: vec![],
    };

    assert!(ptr::eq(&item.first, item.as_ref()));
    assert!(ptr::eq(&item.second, item.as_ref()));
}

#[derive(AsRef)]
struct SingleFieldGenericStruct<T> {
    first: T,
}

#[test]
fn single_field_generic_struct() {
    let item = SingleFieldGenericStruct {
        first: String::from("test"),
    };

    assert!(ptr::eq(&item.first, item.as_ref()));
}

#[derive(AsRef)]
struct MultiFieldGenericStruct<T, U> {
    #[as_ref]
    first: Vec<T>,
    #[as_ref]
    second: [U; 2],
    third: Vec<usize>,
}

#[test]
fn multi_field_generic_struct() {
    let item = MultiFieldGenericStruct {
        first: b"test".to_vec(),
        second: [0i32, 1i32],
        third: vec![],
    };

    assert!(ptr::eq(&item.first, item.as_ref()));
    assert!(ptr::eq(&item.second, item.as_ref()));
}
