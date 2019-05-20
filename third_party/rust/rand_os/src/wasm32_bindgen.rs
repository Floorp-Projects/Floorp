// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Implementation for WASM via wasm-bindgen

use rand_core::{Error, ErrorKind};
use super::OsRngImpl;
use super::__wbg_shims::*;

use wasm_bindgen::prelude::*;


#[derive(Clone, Debug)]
pub enum OsRng {
    Node(NodeCrypto),
    Browser(BrowserCrypto),
}

impl OsRngImpl for OsRng {
    fn new() -> Result<OsRng, Error> {
        // First up we need to detect if we're running in node.js or a
        // browser. To do this we get ahold of the `this` object (in a bit
        // of a roundabout fashion).
        //
        // Once we have `this` we look at its `self` property, which is
        // only defined on the web (either a main window or web worker).
        let this = Function::new("return this").call(&JsValue::undefined());
        assert!(this != JsValue::undefined());
        let this = This::from(this);
        let is_browser = this.self_() != JsValue::undefined();

        if !is_browser {
            return Ok(OsRng::Node(node_require("crypto")))
        }

        // If `self` is defined then we're in a browser somehow (main window
        // or web worker). Here we want to try to use
        // `crypto.getRandomValues`, but if `crypto` isn't defined we assume
        // we're in an older web browser and the OS RNG isn't available.
        let crypto = this.crypto();
        if crypto.is_undefined() {
            let msg = "self.crypto is undefined";
            return Err(Error::new(ErrorKind::Unavailable, msg))
        }

        // Test if `crypto.getRandomValues` is undefined as well
        let crypto: BrowserCrypto = crypto.into();
        if crypto.get_random_values_fn().is_undefined() {
            let msg = "crypto.getRandomValues is undefined";
            return Err(Error::new(ErrorKind::Unavailable, msg))
        }

        // Ok! `self.crypto.getRandomValues` is a defined value, so let's
        // assume we can do browser crypto.
        Ok(OsRng::Browser(crypto))
    }

    fn fill_chunk(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        match *self {
            OsRng::Node(ref n) => n.random_fill_sync(dest),
            OsRng::Browser(ref n) => n.get_random_values(dest),
        }
        Ok(())
    }

    fn max_chunk_size(&self) -> usize {
        match *self {
            OsRng::Node(_) => usize::max_value(),
            OsRng::Browser(_) => {
                // see https://developer.mozilla.org/en-US/docs/Web/API/Crypto/getRandomValues
                //
                // where it says:
                //
                // > A QuotaExceededError DOMException is thrown if the
                // > requested length is greater than 65536 bytes.
                65536
            }
        }
    }

    fn method_str(&self) -> &'static str {
        match *self {
            OsRng::Node(_) => "crypto.randomFillSync",
            OsRng::Browser(_) => "crypto.getRandomValues",
        }
    }
}
