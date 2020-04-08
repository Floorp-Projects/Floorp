/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::fs::File;
use std::io::prelude::*;
use std::panic;
use std::path::Path;
extern crate webrtc_sdp;

// Takes the filename of a file that contains SDP, and optionally the trailing
// flag --expect-failure.
// If --expect-failure is passed, then the program will exit with a successful
// exit code if the file fails to parse.
fn main() {
    let mut args = std::env::args();
    let filename = match args.nth(1) {
        None => {
            eprintln!("Missing file name argument!");
            std::process::exit(1);
        }
        Some(x) => x,
    };

    let path = Path::new(filename.as_str());
    let display = path.display();

    let mut file = match File::open(&path) {
        Err(why) => panic!("Failed to open {}: {}", display, why),
        Ok(file) => file,
    };

    let mut s = String::new();
    match file.read_to_string(&mut s) {
        Err(why) => panic!("Couldn't read {}: {}", display, why),
        Ok(s) => s,
    };

    // Hook up the panic handler if it is expected to fail to parse
    let expect_failure = if let Some(x) = args.next() {
        if x.to_lowercase() != "--expect-failure" {
            eprintln!("Extra arguments passed!");
            std::process::exit(1);
        }
        panic::set_hook(Box::new(|_| {
            println!("Exited with failure, as expected.");
            std::process::exit(0);
        }));
        true
    } else {
        false
    };

    // Remove comment lines
    let s = s
        .lines()
        .filter(|&l| !l.trim_start().starts_with(';'))
        .collect::<Vec<&str>>()
        .join("\r\n");

    if let Err(why) = webrtc_sdp::parse_sdp(&s, true) {
        panic!("Failed to parse SDP with error: {}", why);
    }

    if expect_failure {
        eprintln!("Successfully parsed SDP that was expected to fail. You may need to update the example expectations.");
        std::process::exit(1);
    }
    println!("Successfully parsed SDP");
}
