/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// This crate exists to test managing the Rust Arc strong counts as JS objects are
/// created/destroyed.  See `test_refcounts.js` for how it's used.
use std::sync::{Arc, Mutex};

pub struct SingletonObject;

impl SingletonObject {
    pub fn method(&self) {}
}

static SINGLETON: Mutex<Option<Arc<SingletonObject>>> = Mutex::new(None);

pub fn get_singleton() -> Arc<SingletonObject> {
    Arc::clone(
        SINGLETON
            .lock()
            .unwrap()
            .get_or_insert_with(|| Arc::new(SingletonObject)),
    )
}

pub fn get_js_refcount() -> i32 {
    // Subtract 2: one for the reference in the Mutex and one for the temporary reference that
    // we're calling Arc::strong_count on.
    (Arc::strong_count(&get_singleton()) as i32) - 2
}

include!(concat!(env!("OUT_DIR"), "/refcounts.uniffi.rs"));
