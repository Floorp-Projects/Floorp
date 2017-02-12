/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[macro_use] extern crate quote;
extern crate syn;

use std::env;
use std::path::Path;


#[cfg(feature = "dummy_match_byte")]
mod codegen {
    use std::path::Path;
    pub fn main(_: &Path) {}
}

#[cfg(not(feature = "dummy_match_byte"))]
#[path = "src/macros/mod.rs"]
mod macros;

#[cfg(not(feature = "dummy_match_byte"))]
mod codegen {
    use macros;
    use std::env;
    use std::path::Path;

    pub fn main(tokenizer_rs: &Path) {
        macros::match_byte::expand(tokenizer_rs,
                                   &Path::new(&env::var("OUT_DIR").unwrap()).join("tokenizer.rs"));

    }
}

fn main() {
    let manifest_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let tokenizer_rs = Path::new(&manifest_dir).join("src/tokenizer.rs");
    codegen::main(&tokenizer_rs);
    println!("cargo:rerun-if-changed={}", tokenizer_rs.display());
}
