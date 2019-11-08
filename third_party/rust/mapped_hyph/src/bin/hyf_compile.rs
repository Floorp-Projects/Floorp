/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

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
