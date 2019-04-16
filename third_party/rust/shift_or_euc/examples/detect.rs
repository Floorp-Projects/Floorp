// Copyright 2018 Mozilla Foundation. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::fs::File;
use std::io::Read;

use shift_or_euc::Detector;

fn main() {
    let mut args = std::env::args_os();
    if args.next().is_none() {
        eprintln!("Error: Program name missing from arguments.");
        std::process::exit(-1);
    }
    if let Some(path) = args.next() {
        if args.next().is_some() {
            eprintln!("Error: Too many arguments.");
            std::process::exit(-3);
        }
        if let Ok(mut file) = File::open(path) {
            let mut buffer = [0u8; 4096];
            let mut detector = Detector::new(true);
            loop {
                if let Ok(num_read) = file.read(&mut buffer[..]) {
                    let opt_enc = if num_read == 0 {
                        detector.feed(b"", true)
                    } else {
                        detector.feed(&buffer[..num_read], false)
                    };
                    if let Some(encoding) = opt_enc {
                        println!("{}", encoding.name());
                        return;
                    } else if num_read == 0 {
                        println!("Undecided");
                        return;
                    }
                } else {
                    eprintln!("Error: Error reading file.");
                    std::process::exit(-5);
                }
            }
        } else {
            eprintln!("Error: Could not open file.");
            std::process::exit(-4);
        }
    } else {
        eprintln!("Error: One path argument needed.");
        std::process::exit(-2);
    }
}
