use std::collections::HashMap;

use ron::from_str;
use serde::Deserialize;

#[derive(Deserialize)]
struct Newtype(i32);

#[derive(Deserialize)]
struct Tuple(i32, i32);

#[derive(Deserialize)]
#[allow(dead_code)]
struct Struct {
    a: i32,
    b: i32,
}

#[derive(Deserialize)]
#[allow(dead_code)]
enum Enum {
    Newtype(i32),
    Tuple(i32, i32),
    Struct { a: i32, b: i32 },
}

#[test]
fn test_trailing_comma_some() {
    assert!(from_str::<Option<i32>>("Some(1)").is_ok());
    assert!(from_str::<Option<i32>>("Some(1,)").is_ok());
    assert!(from_str::<Option<i32>>("Some(1,,)").is_err());
}

#[test]
fn test_trailing_comma_tuple() {
    assert!(from_str::<(i32, i32)>("(1,2)").is_ok());
    assert!(from_str::<(i32, i32)>("(1,2,)").is_ok());
    assert!(from_str::<(i32, i32)>("(1,2,,)").is_err());
}

#[test]
fn test_trailing_comma_list() {
    assert!(from_str::<Vec<i32>>("[1,2]").is_ok());
    assert!(from_str::<Vec<i32>>("[1,2,]").is_ok());
    assert!(from_str::<Vec<i32>>("[1,2,,]").is_err());
}

#[test]
fn test_trailing_comma_map() {
    assert!(from_str::<HashMap<i32, bool>>("{1:false,2:true}").is_ok());
    assert!(from_str::<HashMap<i32, bool>>("{1:false,2:true,}").is_ok());
    assert!(from_str::<HashMap<i32, bool>>("{1:false,2:true,,}").is_err());
}

#[test]
fn test_trailing_comma_newtype_struct() {
    assert!(from_str::<Newtype>("(1)").is_ok());
    assert!(from_str::<Newtype>("(1,)").is_ok());
    assert!(from_str::<Newtype>("(1,,)").is_err());
}

#[test]
fn test_trailing_comma_tuple_struct() {
    assert!(from_str::<Tuple>("(1,2)").is_ok());
    assert!(from_str::<Tuple>("(1,2,)").is_ok());
    assert!(from_str::<Tuple>("(1,2,,)").is_err());
}

#[test]
fn test_trailing_comma_struct() {
    assert!(from_str::<Struct>("(a:1,b:2)").is_ok());
    assert!(from_str::<Struct>("(a:1,b:2,)").is_ok());
    assert!(from_str::<Struct>("(a:1,b:2,,)").is_err());
}

#[test]
fn test_trailing_comma_enum_newtype_variant() {
    assert!(from_str::<Enum>("Newtype(1)").is_ok());
    assert!(from_str::<Enum>("Newtype(1,)").is_ok());
    assert!(from_str::<Enum>("Newtype(1,,)").is_err());
}

#[test]
fn test_trailing_comma_enum_tuple_variant() {
    assert!(from_str::<Enum>("Tuple(1,2)").is_ok());
    assert!(from_str::<Enum>("Tuple(1,2,)").is_ok());
    assert!(from_str::<Enum>("Tuple(1,2,,)").is_err());
}

#[test]
fn test_trailing_comma_enum_struct_variant() {
    assert!(from_str::<Enum>("Struct(a:1,b:2)").is_ok());
    assert!(from_str::<Enum>("Struct(a:1,b:2,)").is_ok());
    assert!(from_str::<Enum>("Struct(a:1,b:2,,)").is_err());
}
