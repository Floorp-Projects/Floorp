#![allow(dead_code)]

#[macro_use]
extern crate derive_more;

use std::path::PathBuf;
use std::ptr;

#[derive(AsMut)]
struct SingleFieldTuple(String);

#[test]
fn single_field_tuple() {
    let mut item = SingleFieldTuple(String::from("test"));

    assert!(ptr::eq(&mut item.0, item.as_mut()));
}

#[derive(AsMut)]
#[as_mut(forward)]
struct SingleFieldForward(Vec<i32>);

#[test]
fn single_field_forward() {
    let mut item = SingleFieldForward(vec![]);
    let _: &mut [i32] = (&mut item).as_mut();
}

#[derive(AsMut)]
struct SingleFieldStruct {
    first: String,
}

#[test]
fn single_field_struct() {
    let mut item = SingleFieldStruct {
        first: String::from("test"),
    };

    assert!(ptr::eq(&mut item.first, item.as_mut()));
}

#[derive(AsMut)]
struct MultiFieldTuple(#[as_mut] String, #[as_mut] PathBuf, Vec<usize>);

#[test]
fn multi_field_tuple() {
    let mut item = MultiFieldTuple(String::from("test"), PathBuf::new(), vec![]);

    assert!(ptr::eq(&mut item.0, item.as_mut()));
    assert!(ptr::eq(&mut item.1, item.as_mut()));
}

#[derive(AsMut)]
struct MultiFieldStruct {
    #[as_mut]
    first: String,
    #[as_mut]
    second: PathBuf,
    third: Vec<usize>,
}

#[test]
fn multi_field_struct() {
    let mut item = MultiFieldStruct {
        first: String::from("test"),
        second: PathBuf::new(),
        third: vec![],
    };

    assert!(ptr::eq(&mut item.first, item.as_mut()));
    assert!(ptr::eq(&mut item.second, item.as_mut()));
}

#[derive(AsMut)]
struct SingleFieldGenericStruct<T> {
    first: T,
}

#[test]
fn single_field_generic_struct() {
    let mut item = SingleFieldGenericStruct {
        first: String::from("test"),
    };

    assert!(ptr::eq(&mut item.first, item.as_mut()));
}

#[derive(AsMut)]
struct MultiFieldGenericStruct<T> {
    #[as_mut]
    first: Vec<T>,
    #[as_mut]
    second: PathBuf,
    third: Vec<usize>,
}

#[test]
fn multi_field_generic_struct() {
    let mut item = MultiFieldGenericStruct {
        first: b"test".to_vec(),
        second: PathBuf::new(),
        third: vec![],
    };

    assert!(ptr::eq(&mut item.first, item.as_mut()));
    assert!(ptr::eq(&mut item.second, item.as_mut()));
}
