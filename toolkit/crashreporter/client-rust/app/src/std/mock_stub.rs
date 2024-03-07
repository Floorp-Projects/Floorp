/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#![allow(dead_code)]

#[inline(always)]
pub fn hook<T: std::any::Any + Send + Sync + Clone>(normally: T, _name: &'static str) -> T {
    normally
}

#[inline(always)]
pub fn try_hook<T: std::any::Any + Send + Sync + Clone>(fallback: T, _name: &'static str) -> T {
    fallback
}
