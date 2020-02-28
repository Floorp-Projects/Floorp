extern crate bumpalo;
use bumpalo::{collections::String, Bump};
use std::fmt::Write;

#[test]
fn format_a_bunch_of_strings() {
    let b = Bump::new();
    let mut s = String::from_str_in("hello", &b);
    for i in 0..1000 {
        write!(&mut s, " {}", i).unwrap();
    }
}
