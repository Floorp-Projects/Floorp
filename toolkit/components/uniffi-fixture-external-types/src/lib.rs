/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pub fn gradient(value: Option<uniffi_geometry::Line>) -> f64 {
    match value {
        None => 0.0,
        Some(value) => uniffi_geometry::gradient(value),
    }
}

include!(concat!(env!("OUT_DIR"), "/external-types.uniffi.rs"));
