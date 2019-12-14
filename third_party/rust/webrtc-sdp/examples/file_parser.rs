/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::env;
use std::error::Error;
use std::fs::File;
use std::io::prelude::*;
use std::path::Path;
extern crate webrtc_sdp;

fn main() {
    let filename = match env::args().nth(1) {
        None => {
            eprintln!("Missing file name argument!");
            std::process::exit(1);
        }
        Some(x) => x,
    };
    let path = Path::new(filename.as_str());
    let display = path.display();

    let mut file = match File::open(&path) {
        Err(why) => panic!("Failed to open {}: {}", display, why.description()),
        Ok(file) => file,
    };

    let mut s = String::new();
    match file.read_to_string(&mut s) {
        Err(why) => panic!("Couldn't read {}: {}", display, why.description()),
        Ok(s) => s,
    };

    if let Err(why) = webrtc_sdp::parse_sdp(&s, true) {
        panic!("Failed to parse SDP with error: {}", why);
    }
    println!("Successfully parsed SDP");
}
