extern crate same_file;

use std::error::Error;
use same_file::is_same_file;

fn try_main() -> Result<(), Box<Error>> {
    assert!(is_same_file("/bin/sh", "/usr/bin/sh")?);
    Ok(()) 
}

fn main() {
    try_main().unwrap();
}
