// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Implementation for WASM via stdweb

use std::mem;
use stdweb::unstable::TryInto;
use stdweb::web::error::Error as WebError;
use rand_core::{Error, ErrorKind};
use super::OsRngImpl;

#[derive(Clone, Debug)]
enum OsRngMethod {
    Browser,
    Node
}

#[derive(Clone, Debug)]
pub struct OsRng(OsRngMethod);

impl OsRngImpl for OsRng {
    fn new() -> Result<OsRng, Error> {
        let result = js! {
            try {
                if (
                    typeof self === "object" &&
                    typeof self.crypto === "object" &&
                    typeof self.crypto.getRandomValues === "function"
                ) {
                    return { success: true, ty: 1 };
                }

                if (typeof require("crypto").randomBytes === "function") {
                    return { success: true, ty: 2 };
                }

                return { success: false, error: new Error("not supported") };
            } catch(err) {
                return { success: false, error: err };
            }
        };

        if js!{ return @{ result.as_ref() }.success } == true {
            let ty = js!{ return @{ result }.ty };

            if ty == 1 { Ok(OsRng(OsRngMethod::Browser)) }
            else if ty == 2 { Ok(OsRng(OsRngMethod::Node)) }
            else { unreachable!() }
        } else {
            let err: WebError = js!{ return @{ result }.error }.try_into().unwrap();
            Err(Error::with_cause(ErrorKind::Unavailable, "WASM Error", err))
        }
    }


    fn fill_chunk(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        assert_eq!(mem::size_of::<usize>(), 4);

        let len = dest.len() as u32;
        let ptr = dest.as_mut_ptr() as i32;

        let result = match self.0 {
            OsRngMethod::Browser => js! {
                try {
                    let array = new Uint8Array(@{ len });
                    self.crypto.getRandomValues(array);
                    HEAPU8.set(array, @{ ptr });

                    return { success: true };
                } catch(err) {
                    return { success: false, error: err };
                }
            },
            OsRngMethod::Node => js! {
                try {
                    let bytes = require("crypto").randomBytes(@{ len });
                    HEAPU8.set(new Uint8Array(bytes), @{ ptr });

                    return { success: true };
                } catch(err) {
                    return { success: false, error: err };
                }
            }
        };

        if js!{ return @{ result.as_ref() }.success } == true {
            Ok(())
        } else {
            let err: WebError = js!{ return @{ result }.error }.try_into().unwrap();
            Err(Error::with_cause(ErrorKind::Unexpected, "WASM Error", err))
        }
    }

    fn max_chunk_size(&self) -> usize { 65536 }

    fn method_str(&self) -> &'static str {
        match self.0 {
            OsRngMethod::Browser => "Crypto.getRandomValues",
            OsRngMethod::Node => "crypto.randomBytes",
        }
    }
}
