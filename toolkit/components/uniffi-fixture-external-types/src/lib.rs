/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use uniffi_geometry::{Line, Point};

pub fn gradient(value: Option<Line>) -> f64 {
    match value {
        None => 0.0,
        Some(value) => uniffi_geometry::gradient(value),
    }
}

pub fn intersection(ln1: Line, ln2: Line) -> Option<Point> {
    uniffi_geometry::intersection(ln1, ln2)
}

include!(concat!(env!("OUT_DIR"), "/external-types.uniffi.rs"));
