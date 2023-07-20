/* This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// Extension traits that are shared across multiple render targets
use crate::Config;
use extend::ext;
use uniffi_bindgen::interface::{Function, Method, Object};

/// Check if a JS function should be async.
///
/// `uniffi-bindgen-gecko-js` has special async handling.  Many non-async Rust functions end up
/// being async in js
fn is_js_async(config: &Config, spec: &str) -> bool {
    if config.receiver_thread.main.contains(spec) {
        false
    } else if config.receiver_thread.worker.contains(spec) {
        true
    } else {
        match &config.receiver_thread.default {
            Some(t) => t != "main",
            _ => true,
        }
    }
}

#[ext]
pub impl Function {
    fn is_js_async(&self, config: &Config) -> bool {
        is_js_async(config, self.name())
    }
}

#[ext]
pub impl Object {
    fn is_constructor_async(&self, config: &Config) -> bool {
        is_js_async(config, self.name())
    }

    fn is_method_async(&self, method: &Method, config: &Config) -> bool {
        is_js_async(config, &format!("{}.{}", self.name(), method.name()))
    }
}
