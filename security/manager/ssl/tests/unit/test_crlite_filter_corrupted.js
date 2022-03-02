// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Tests that CRLite is left in the uninitialized state when the profile
// contains a corrupted filter file.
//
// There are many ways that a filter file could be corrupted, but the parsing
// is done in rust-cascade, not cert_storage, so it is sufficient for us to
// test any form of corruption here. For simplicity we just try to load a
// single \x00 byte as the filter.

"use strict";

/* eslint-disable no-unused-vars */
let coverage = do_get_file("test_crlite_preexisting/crlite.coverage");
let enrollment = do_get_file("test_crlite_preexisting/crlite.enrollment");
let filter = do_get_file("test_crlite_corrupted/hash-alg-0.filter");

load("./corrupted_crlite_helper.js");
