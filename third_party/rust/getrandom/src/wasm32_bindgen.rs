// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Implementation for WASM via wasm-bindgen
extern crate std;

use core::cell::RefCell;
use core::mem;
use core::num::NonZeroU32;
use std::thread_local;

use wasm_bindgen::prelude::*;

use crate::Error;
use crate::error::CODE_PREFIX;
use crate::utils::use_init;

const CODE_CRYPTO_UNDEF: u32 = CODE_PREFIX | 0x80;
const CODE_GRV_UNDEF: u32 = CODE_PREFIX | 0x81;

#[derive(Clone, Debug)]
enum RngSource {
    Node(NodeCrypto),
    Browser(BrowserCrypto),
}

thread_local!(
    static RNG_SOURCE: RefCell<Option<RngSource>> = RefCell::new(None);
);

pub fn getrandom_inner(dest: &mut [u8]) -> Result<(), Error> {
    assert_eq!(mem::size_of::<usize>(), 4);

    RNG_SOURCE.with(|f| {
        use_init(f, getrandom_init, |source| {
            match *source {
                RngSource::Node(ref n) => n.random_fill_sync(dest),
                RngSource::Browser(ref n) => {
                    // see https://developer.mozilla.org/en-US/docs/Web/API/Crypto/getRandomValues
                    //
                    // where it says:
                    //
                    // > A QuotaExceededError DOMException is thrown if the
                    // > requested length is greater than 65536 bytes.
                    for chunk in dest.chunks_mut(65536) {
                        n.get_random_values(chunk)
                    }
                }
            }
            Ok(())
        })
    })

}

fn getrandom_init() -> Result<RngSource, Error> {
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
        return Ok(RngSource::Node(node_require("crypto")))
    }

    // If `self` is defined then we're in a browser somehow (main window
    // or web worker). Here we want to try to use
    // `crypto.getRandomValues`, but if `crypto` isn't defined we assume
    // we're in an older web browser and the OS RNG isn't available.
    let crypto = this.crypto();
    if crypto.is_undefined() {
        return Err(Error::from(unsafe {
            NonZeroU32::new_unchecked(CODE_CRYPTO_UNDEF)
        }));
    }

    // Test if `crypto.getRandomValues` is undefined as well
    let crypto: BrowserCrypto = crypto.into();
    if crypto.get_random_values_fn().is_undefined() {
        return Err(Error::from(unsafe {
            NonZeroU32::new_unchecked(CODE_GRV_UNDEF)
        }));
    }

    // Ok! `self.crypto.getRandomValues` is a defined value, so let's
    // assume we can do browser crypto.
    Ok(RngSource::Browser(crypto))
}

#[inline(always)]
pub fn error_msg_inner(n: NonZeroU32) -> Option<&'static str> {
    match n.get() {
        CODE_CRYPTO_UNDEF => Some("getrandom: self.crypto is undefined"),
        CODE_GRV_UNDEF => Some("crypto.getRandomValues is undefined"),
        _ => None
    }
}

#[wasm_bindgen]
extern "C" {
    type Function;
    #[wasm_bindgen(constructor)]
    fn new(s: &str) -> Function;
    #[wasm_bindgen(method)]
    fn call(this: &Function, self_: &JsValue) -> JsValue;

    type This;
    #[wasm_bindgen(method, getter, structural, js_name = self)]
    fn self_(me: &This) -> JsValue;
    #[wasm_bindgen(method, getter, structural)]
    fn crypto(me: &This) -> JsValue;

    #[derive(Clone, Debug)]
    type BrowserCrypto;

    // TODO: these `structural` annotations here ideally wouldn't be here to
    // avoid a JS shim, but for now with feature detection they're
    // unavoidable.
    #[wasm_bindgen(method, js_name = getRandomValues, structural, getter)]
    fn get_random_values_fn(me: &BrowserCrypto) -> JsValue;
    #[wasm_bindgen(method, js_name = getRandomValues, structural)]
    fn get_random_values(me: &BrowserCrypto, buf: &mut [u8]);

    #[wasm_bindgen(js_name = require)]
    fn node_require(s: &str) -> NodeCrypto;

    #[derive(Clone, Debug)]
    type NodeCrypto;

    #[wasm_bindgen(method, js_name = randomFillSync, structural)]
    fn random_fill_sync(me: &NodeCrypto, buf: &mut [u8]);
}
