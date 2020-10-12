// Copyright 2019-2020 Mozilla Foundation. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

extern crate mapped_hyph;
extern crate env_logger;

use std::env;
use std::fs::File;

fn main() -> std::io::Result<()> {
    env_logger::init();
    let args: Vec<String> = env::args().collect();
    if args.len() == 3 {
        let in_file = File::open(&args[1])?;
        let mut out_file = File::create(&args[2])?;
        mapped_hyph::builder::compile(&in_file, &mut out_file, true)?;
    } else {
        println!("usage: hyf_compile <pattern-file> <output-file>");
    }
    Ok(())
}
