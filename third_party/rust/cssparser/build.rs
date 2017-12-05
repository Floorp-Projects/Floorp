/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate quote;
extern crate syn;

use std::env;
use std::path::Path;


#[cfg(feature = "dummy_match_byte")]
mod codegen {
    use std::path::Path;
    pub fn main(_: &Path) {}
}

#[cfg(not(feature = "dummy_match_byte"))]
#[path = "build/match_byte.rs"]
mod match_byte;

#[cfg(not(feature = "dummy_match_byte"))]
mod codegen {
    use match_byte;
    use std::env;
    use std::path::Path;

    pub fn main(tokenizer_rs: &Path) {
        match_byte::expand(tokenizer_rs,
                           &Path::new(&env::var("OUT_DIR").unwrap()).join("tokenizer.rs"));

    }
}

fn main() {
    if std::mem::size_of::<Option<bool>>() == 1 {
        // https://github.com/rust-lang/rust/pull/45225
        println!("cargo:rustc-cfg=rustc_has_pr45225")
    }

    let manifest_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let tokenizer_rs = Path::new(&manifest_dir).join("src/tokenizer.rs");
    codegen::main(&tokenizer_rs);
    println!("cargo:rerun-if-changed={}", tokenizer_rs.display());
}
