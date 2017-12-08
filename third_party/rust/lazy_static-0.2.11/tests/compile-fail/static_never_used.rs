// error-pattern:static item is never used: `UNUSED`
#![deny(dead_code)]
#[macro_use]
extern crate lazy_static;

lazy_static! {
    static ref UNUSED: () = ();
}

fn main() { }
