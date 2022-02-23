/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![allow(bad_style)]

use crate::interfaces::*;
use crate::*;

// NOTE: This file contains a series of `include!()` invocations, defining all
// idl interfaces directly within this module.
include!(mozbuild::objdir_path!("dist/xpcrs/rt/all.rs"));
