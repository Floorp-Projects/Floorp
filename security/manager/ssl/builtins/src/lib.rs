/* -*- Mode: rust; rust-indent-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// certdata may include dead code depending on the contents of certdata.txt
#[allow(dead_code)]
mod certdata;
mod internal;
mod pkcs11;
mod version;
