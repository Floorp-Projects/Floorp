extern crate serde;
extern crate serde_with_macros;

use serde::Serialize;
use serde_with_macros::skip_serializing_none;

#[skip_serializing_none]
#[derive(Serialize)]
struct Data {
    #[serialize_always]
    #[serde(skip_serializing_if = "Option::is_none")]
    a: Option<char>,
}

#[skip_serializing_none]
#[derive(Serialize)]
struct Data2(
    #[serialize_always]
    #[serde(skip_serializing_if = "Option::is_none")]
    Option<char>,
);

#[skip_serializing_none]
#[derive(Serialize)]
struct Data3 {
    #[serialize_always]
    a: char,
}

fn main() {}
