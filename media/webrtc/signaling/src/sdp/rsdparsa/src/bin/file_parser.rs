use std::error::Error;
use std::io::prelude::*;
use std::fs::File;
use std::path::Path;
use std::env;
extern crate rsdparsa;

fn main() {
    let filename = match env::args().nth(1) {
        None => {
            println!("Missing file name argument!");
            return;
        },
        Some(x) => x,
    };
    let path = Path::new(filename.as_str());
    let display = path.display();

    let mut file = match File::open(&path) {
        Err(why) => panic!("Failed to open {}: {}",
                             display,
                             why.description()),
        Ok(file) => file
    };

    let mut s = String::new();
    match file.read_to_string(&mut s) {
        Err(why) => panic!("couldn't read {}: {}",
                           display,
                           why.description()),
        Ok(s) => s
    };

    rsdparsa::parse_sdp(&s, true).is_ok();
}
