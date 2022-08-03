/* This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// Extension traits that are shared across multiple render targets
use extend::ext;
use uniffi_bindgen::interface::{Constructor, FFIFunction, Function, Method};

#[ext]
pub impl FFIFunction {
    fn is_async(&self) -> bool {
        // TODO check `uniffi.toml` or some other configuration to figure this out
        true
    }
}

#[ext]
pub impl Function {
    fn is_async(&self) -> bool {
        // TODO check `uniffi.toml` or some other configuration to figure this out
        true
    }
}

#[ext]
pub impl Constructor {
    fn is_async(&self) -> bool {
        // TODO check `uniffi.toml` or some other configuration to figure this out
        true
    }
}

#[ext]
pub impl Method {
    fn is_async(&self) -> bool {
        // TODO check `uniffi.toml` or some other configuration to figure this out
        true
    }
}
