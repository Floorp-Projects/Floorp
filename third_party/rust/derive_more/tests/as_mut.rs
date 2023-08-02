#![cfg_attr(not(feature = "std"), no_std)]
#![allow(dead_code)]

#[cfg(not(feature = "std"))]
extern crate alloc;

#[cfg(not(feature = "std"))]
use alloc::{string::String, vec, vec::Vec};
use core::ptr;

use derive_more::AsMut;

#[derive(AsMut)]
struct SingleFieldTuple(String);

#[test]
fn single_field_tuple() {
    let mut item = SingleFieldTuple("test".into());

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
        first: "test".into(),
    };

    assert!(ptr::eq(&mut item.first, item.as_mut()));
}

#[cfg(feature = "std")]
mod pathbuf {
    use std::path::PathBuf;

    use super::*;

    #[derive(AsMut)]
    struct MultiFieldTuple(#[as_mut] String, #[as_mut] PathBuf, Vec<usize>);

    #[test]
    fn multi_field_tuple() {
        let mut item = MultiFieldTuple("test".into(), PathBuf::new(), vec![]);

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
            first: "test".into(),
            second: PathBuf::new(),
            third: vec![],
        };

        assert!(ptr::eq(&mut item.first, item.as_mut()));
        assert!(ptr::eq(&mut item.second, item.as_mut()));
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
}

#[derive(AsMut)]
struct SingleFieldGenericStruct<T> {
    first: T,
}

#[test]
fn single_field_generic_struct() {
    let mut item = SingleFieldGenericStruct { first: "test" };

    assert!(ptr::eq(&mut item.first, item.as_mut()));
}
