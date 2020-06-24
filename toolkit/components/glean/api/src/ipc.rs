// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! IPC Implementation, Rust part

use crate::metrics::CommonMetricData;
use once_cell::sync::Lazy;
use std::collections::HashMap;
use std::sync::Mutex;

/// Contains all the information necessary to update the metrics on the main
/// process.
#[derive(Debug)]
pub struct IPCPayload {}

/// Uniquely identifies a single metric within its metric type.
#[derive(Debug, PartialEq, Eq, Hash, Clone)]
pub struct MetricId {
    category: String,
    name: String,
}

impl MetricId {
    pub fn new(meta: CommonMetricData) -> Self {
        Self {
            category: meta.category,
            name: meta.name,
        }
    }
}

/// Global singleton: pending IPC payload.
static PAYLOAD: Lazy<Mutex<IPCPayload>> = Lazy::new(|| Mutex::new(IPCPayload {}));

pub fn with_ipc_payload<F, R>(f: F) -> R
where
    F: FnOnce(&mut IPCPayload) -> R,
{
    let mut payload = PAYLOAD.lock().unwrap();
    f(&mut payload)
}

/// Do we need IPC?
///
/// Thread-safe.
pub fn need_ipc() -> bool {
    false
}
