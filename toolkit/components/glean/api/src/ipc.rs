// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! IPC Implementation, Rust part

use crate::private::{Instant, MetricId};
use once_cell::sync::Lazy;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
#[cfg(not(feature = "with_gecko"))]
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Mutex;
#[cfg(feature = "with_gecko")]
use {
    std::sync::atomic::{AtomicU32, Ordering},
    xpcom::interfaces::nsIXULRuntime,
};

use super::metrics::__glean_metric_maps;

type EventRecord = (Instant, Option<HashMap<i32, String>>);

/// Contains all the information necessary to update the metrics on the main
/// process.
#[derive(Debug, Default, Deserialize, Serialize)]
pub struct IPCPayload {
    pub counters: HashMap<MetricId, i32>,
    pub events: HashMap<MetricId, Vec<EventRecord>>,
    pub memory_samples: HashMap<MetricId, Vec<u64>>,
    pub string_lists: HashMap<MetricId, Vec<String>>,
    pub timing_samples: HashMap<MetricId, Vec<u128>>,
}

/// Global singleton: pending IPC payload.
static PAYLOAD: Lazy<Mutex<IPCPayload>> = Lazy::new(|| Mutex::new(IPCPayload::default()));

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
    if let Some(appinfo) = xpcom::services::get_XULRuntime() {
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

/// An RAII that, on drop, restores the value used to determine whether FOG
/// needs IPC. Used in tests.
/// ```rust,ignore
/// #[test]
/// fn test_need_ipc_raii() {
///     assert!(false == ipc::need_ipc());
///     {
///         let _raii = ipc::test_set_need_ipc(true);
///         assert!(ipc::need_ipc());
///     }
///     assert!(false == ipc::need_ipc());
/// }
/// ```
#[cfg(not(feature = "with_gecko"))]
pub struct TestNeedIpcRAII {
    prev_value: bool,
}

#[cfg(not(feature = "with_gecko"))]
impl Drop for TestNeedIpcRAII {
    fn drop(&mut self) {
        TEST_NEED_IPC.store(self.prev_value, Ordering::Relaxed);
    }
}

#[cfg(not(feature = "with_gecko"))]
static TEST_NEED_IPC: AtomicBool = AtomicBool::new(false);

/// Test-only API for telling FOG to use IPC mechanisms even if the test has
/// only the one process. See TestNeedIpcRAII for an example.
#[cfg(not(feature = "with_gecko"))]
pub fn test_set_need_ipc(need_ipc: bool) -> TestNeedIpcRAII {
    TestNeedIpcRAII {
        prev_value: TEST_NEED_IPC.swap(need_ipc, Ordering::Relaxed),
    }
}

#[cfg(not(feature = "with_gecko"))]
pub fn need_ipc() -> bool {
    TEST_NEED_IPC.load(Ordering::Relaxed)
}

pub fn take_buf() -> Option<Vec<u8>> {
    with_ipc_payload(move |payload| {
        let buf = bincode::serialize(&payload).ok();
        *payload = IPCPayload {
            ..Default::default()
        };
        buf
    })
}

pub fn replay_from_buf(buf: &[u8]) -> Result<(), ()> {
    let ipc_payload: IPCPayload = bincode::deserialize(buf).map_err(|_| ())?;
    for (id, value) in ipc_payload.counters.into_iter() {
        if let Some(metric) = __glean_metric_maps::COUNTER_MAP.get(&id) {
            metric.add(value);
        }
    }
    for (id, _records) in ipc_payload.events.into_iter() {
        log::info!("Cannot yet replay child process event {:?}", id);
    }
    for (id, samples) in ipc_payload.memory_samples.into_iter() {
        if let Some(metric) = __glean_metric_maps::MEMORY_DISTRIBUTION_MAP.get(&id) {
            samples
                .into_iter()
                .for_each(|sample| metric.accumulate(sample));
        }
    }
    for (id, strings) in ipc_payload.string_lists.into_iter() {
        if let Some(metric) = __glean_metric_maps::STRING_LIST_MAP.get(&id) {
            strings.iter().for_each(|s| metric.add(s));
        }
    }
    for (id, _samples) in ipc_payload.timing_samples.into_iter() {
        log::info!("Cannot yet replay child process timing dist {:?}", id);
    }
    Ok(())
}
