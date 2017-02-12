extern crate same_file;

use same_file::is_same_file;

fn main() {
    assert!(is_same_file("/bin/sh", "/usr/bin/sh").unwrap());
}
