/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// No-op module to permit compiling token HID support for Android, where
// no results are returned.

#![allow(unused_variables)]

pub mod device;
pub mod transaction;
