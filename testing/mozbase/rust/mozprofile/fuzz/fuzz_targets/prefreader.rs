/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![no_main]
use libfuzzer_sys::fuzz_target;
use std::io::Cursor;
extern crate mozprofile;

fuzz_target!(|data: &[u8]| {
    let buf = Vec::new();
    let mut out = Cursor::new(buf);
    mozprofile::prefreader::parse(data).map(|parsed| {
        mozprofile::prefreader::serialize(&parsed, &mut out);
    });
});
