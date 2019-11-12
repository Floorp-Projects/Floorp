// Copyright 2019 Mozilla Foundation. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

extern crate mapped_hyph;

use std::env;
use std::fs::File;

fn main() -> std::io::Result<()> {
    let args: Vec<String> = env::args().collect();
    if args.len() == 3 {
        let in_file = File::open(&args[1])?;
        let mut out_file = File::create(&args[2])?;
        mapped_hyph::builder::write_hyf_file(&mut out_file, mapped_hyph::builder::read_dic_file(&in_file))?;
    } else {
        println!("usage: hyf_compile <pattern-file> <output-file>");
    }
    Ok(())
}
