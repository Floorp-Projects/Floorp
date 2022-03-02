// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Tests that CRLite is left in the uninitialized state when the profile
// contains a corrupted coverage file. Specifically, this handles the case
// where the coverage file is missing.

"use strict";

/* eslint-disable no-unused-vars */
let coverage = undefined;
let enrollment = do_get_file("test_crlite_preexisting/crlite.enrollment");
let filter = do_get_file("test_crlite_filters/20201017-0-filter");

load("./corrupted_crlite_helper.js");
