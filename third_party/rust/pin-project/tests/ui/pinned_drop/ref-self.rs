// `ref (mut) self` are rejected by rustc.

use std::pin::Pin;

pub struct Struct {
    field: u8,
}

impl Struct {
    fn method_ref(ref self: Pin<&mut Self>) {} //~ ERROR expected identifier, found keyword `self`
    fn method_ref_mut(ref mut self: Pin<&mut Self>) {} //~ ERROR expected identifier, found keyword `self`
}

fn main() {}
