/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate nsstring;
extern crate semver;
use nsstring::nsACString;

#[no_mangle]
pub unsafe extern "C" fn GkRustUtils_ParseSemVer(
    ver: &nsACString,
    out_major: *mut u64,
    out_minor: *mut u64,
    out_patch: *mut u64,
) -> bool {
    let version = match semver::Version::parse(&ver.to_utf8()) {
        Ok(ver) => ver,
        Err(_) => return false,
    };
    *out_major = version.major;
    *out_minor = version.minor;
    *out_patch = version.patch;
    true
}
