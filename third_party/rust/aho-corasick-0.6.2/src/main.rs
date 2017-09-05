extern crate memchr;

use std::env;

use lib::AcAutomaton;

#[allow(dead_code)]
mod lib;

fn main() {
    let aut = AcAutomaton::new(env::args().skip(1));
    println!("{}", aut.dot().trim());
}
