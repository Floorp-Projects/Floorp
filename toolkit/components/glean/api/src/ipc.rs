// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! IPC Implementation, Rust part

use crate::metrics::CommonMetricData;
use once_cell::sync::Lazy;
use std::collections::HashMap;
use std::sync::Mutex;
#[cfg(feature = "with_gecko")]
use {
    std::sync::atomic::{AtomicU32, Ordering},
    xpcom::interfaces::nsIXULRuntime,
};

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
#[cfg(feature = "with_gecko")]
static PROCESS_TYPE: Lazy<AtomicU32> = Lazy::new(|| {
    if let Some(appinfo) = xpcom::services::get_AppInfoService() {
        let mut process_type = nsIXULRuntime::PROCESS_TYPE_DEFAULT as u32;
        let rv = unsafe { appinfo.GetProcessType(&mut process_type) };
        if rv.succeeded() {
            return AtomicU32::new(process_type);
        }
    }
    AtomicU32::new(nsIXULRuntime::PROCESS_TYPE_DEFAULT as u32)
});

#[cfg(feature = "with_gecko")]
pub fn need_ipc() -> bool {
    PROCESS_TYPE.load(Ordering::Relaxed) != nsIXULRuntime::PROCESS_TYPE_DEFAULT as u32
}

#[cfg(not(feature = "with_gecko"))]
pub fn need_ipc() -> bool {
    false
}
