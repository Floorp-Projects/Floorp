/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[macro_use]
extern crate quote;
#[macro_use]
extern crate syn;
extern crate proc_macro2;

use std::env;
use std::path::Path;


#[cfg(feature = "dummy_match_byte")]
mod codegen {
    use std::path::Path;
    pub fn main() {}
}

#[cfg(not(feature = "dummy_match_byte"))]
#[path = "build/match_byte.rs"]
mod match_byte;

#[cfg(not(feature = "dummy_match_byte"))]
mod codegen {
    use match_byte;
    use std::env;
    use std::path::Path;
    use std::thread::Builder;

    pub fn main() {
        let manifest_dir = env::var("CARGO_MANIFEST_DIR").unwrap();

        let input = Path::new(&manifest_dir).join("src/tokenizer.rs");
        let output = Path::new(&env::var("OUT_DIR").unwrap()).join("tokenizer.rs");
        println!("cargo:rerun-if-changed={}", input.display());

        // We have stack overflows on Servo's CI.
        let handle = Builder::new().stack_size(128 * 1024 * 1024).spawn(move || {
            match_byte::expand(&input, &output);
        }).unwrap();

        handle.join().unwrap();
    }
}

fn main() {
    if std::mem::size_of::<Option<bool>>() == 1 {
        // https://github.com/rust-lang/rust/pull/45225
        println!("cargo:rustc-cfg=rustc_has_pr45225")
    }

    codegen::main();
}
