/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This module contains convenient accessors for C++ constants.
//!
//! The contents of this file are generated from
//! `toolkit/components/updateagent/UpdateUrlConstants.py`.
include!(concat!(
    env!("MOZ_TOPOBJDIR"),
    "/toolkit/components/updateagent/url_constants.rs"
));
