// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! IPC Implementation, Rust part

use crate::private::MetricId;
use once_cell::sync::Lazy;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
#[cfg(not(feature = "with_gecko"))]
use std::sync::atomic::AtomicBool;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Mutex;
#[cfg(feature = "with_gecko")]
use {std::convert::TryInto, std::sync::atomic::AtomicU32, xpcom::interfaces::nsIXULRuntime};

use super::metrics::__glean_metric_maps;

type EventRecord = (u64, HashMap<String, String>);

/// Contains all the information necessary to update the metrics on the main
/// process.
#[derive(Debug, Default, Deserialize, Serialize)]
pub struct IPCPayload {
    pub counters: HashMap<MetricId, i32>,
    pub custom_samples: HashMap<MetricId, Vec<i64>>,
    pub denominators: HashMap<MetricId, i32>,
    pub events: HashMap<MetricId, Vec<EventRecord>>,
    pub labeled_counters: HashMap<MetricId, HashMap<String, i32>>,
    pub memory_samples: HashMap<MetricId, Vec<u64>>,
    pub numerators: HashMap<MetricId, i32>,
    pub rates: HashMap<MetricId, (i32, i32)>,
    pub string_lists: HashMap<MetricId, Vec<String>>,
    pub timing_samples: HashMap<MetricId, Vec<u64>>,
}

/// Global singleton: pending IPC payload.
static PAYLOAD: Lazy<Mutex<IPCPayload>> = Lazy::new(|| Mutex::new(IPCPayload::default()));
/// Global singleton: number of times the IPC payload was accessed.
static PAYLOAD_ACCESS_COUNT: AtomicUsize = AtomicUsize::new(0);

// The maximum size of an IPC message in Firefox Desktop is 256MB.
// (See IPC::Channel::kMaximumMessageSize)
// In `IPCPayload` the largest size can be attained in the fewest accesses via events.
// Each event could be its own u64 id, u64 timestamp, and HashMap of ten i32 to ten 100-byte strings.
// That's 1056B = 8 + 8 + 10(4 + 100)
// In 256MB we can fit 254200 or so of these, not counting overhead.
// Let's take a conservative estimate of 100000 to
// 0) Account for overhead
// 1) Not be greedy
// 2) Allow time for the dispatch to main thread which will actually perform the flush
// "Why the -1?" Because fetch_add returns the value before the addition.
const PAYLOAD_ACCESS_WATERMARK: usize = 100000 - 1;

pub fn with_ipc_payload<F, R>(f: F) -> R
where
    F: FnOnce(&mut IPCPayload) -> R,
{
    if PAYLOAD_ACCESS_COUNT.fetch_add(1, Ordering::SeqCst) > PAYLOAD_ACCESS_WATERMARK {
        // We reset this before the actual flush to keep all the logic together.
        // Otherwise the count reset would need to happen down in take_buf().
        // This may overcount (resulting in undersized payloads) which is okay.
        PAYLOAD_ACCESS_COUNT.store(0, Ordering::SeqCst);
        handle_payload_filling();
    }
    let mut payload = PAYLOAD.lock().unwrap();
    f(&mut payload)
}

/// Do we need IPC?
///
/// Thread-safe.
#[cfg(feature = "with_gecko")]
static PROCESS_TYPE: Lazy<AtomicU32> = Lazy::new(|| {
    extern "C" {
        fn FOG_GetProcessType() -> i32;
    }
    // SAFETY NOTE: Safe because it returns a primitive by value.
    let process_type = unsafe { FOG_GetProcessType() };
    // It's impossible for i32 to overflow u32, but maybe someone got clever
    // and introduced a negative process type constant. Default to parent.
    let process_type = process_type
        .try_into()
        .unwrap_or(nsIXULRuntime::PROCESS_TYPE_DEFAULT);
    // We don't have process-specific init locations outside of the main
    // process, so we introduce this side-effect to a global static init.
    // This is the absolute first time we decide which process type we're
    // treating this process as, so this is the earliest we can do this.
    register_process_shutdown(process_type);
    AtomicU32::new(process_type)
});

#[cfg(feature = "with_gecko")]
pub fn need_ipc() -> bool {
    PROCESS_TYPE.load(Ordering::Relaxed) != nsIXULRuntime::PROCESS_TYPE_DEFAULT
}

/// The first time we're used in a process,
/// we'll need to start thinking about cleanup.
///
/// Please only call once per process.
/// Multiple calls may register multiple handlers.
#[cfg(feature = "with_gecko")]
fn register_process_shutdown(process_type: u32) {
    match process_type {
        nsIXULRuntime::PROCESS_TYPE_DEFAULT => {
            // Parent process shutdown is handled by the FOG XPCOM Singleton.
        }
        nsIXULRuntime::PROCESS_TYPE_CONTENT => {
            // Content child shutdown is in C++ for access to RunOnShutdown().
            extern "C" {
                fn FOG_RegisterContentChildShutdown();
            }
            unsafe {
                FOG_RegisterContentChildShutdown();
            };
        }
        nsIXULRuntime::PROCESS_TYPE_GMPLUGIN => {
            // GMP process shutdown is handled in GMPChild::ActorDestroy.
        }
        nsIXULRuntime::PROCESS_TYPE_GPU => {
            // GPU process shutdown is handled in GPUParent::ActorDestroy.
        }
        nsIXULRuntime::PROCESS_TYPE_RDD => {
            // RDD process shutdown is handled in RDDParent::ActorDestroy.
        }
        nsIXULRuntime::PROCESS_TYPE_SOCKET => {
            // Socket process shutdown is handled in SocketProcessChild::ActorDestroy.
        }
        nsIXULRuntime::PROCESS_TYPE_UTILITY => {
            // Utility process shutdown is handled in UtilityProcessChild::ActorDestroy.
        }
        _ => {
            // We don't yet support other process types.
            log::error!("Process type {} tried to use FOG, but isn't supported! (Process type constants are in nsIXULRuntime.rs)", process_type);
        }
    }
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

#[cfg(not(feature = "with_gecko"))]
fn handle_payload_filling() {
    // Space intentionally left blank.
    // Without Gecko IPC to drain the buffer, there's nothing we can do.
}

#[cfg(feature = "with_gecko")]
fn handle_payload_filling() {
    extern "C" {
        fn FOG_IPCPayloadFull();
    }
    // SAFETY NOTE: Safe because it doesn't take or return values.
    unsafe { FOG_IPCPayloadFull() };
}

// Reason: We instrument the error counts,
// but don't need more detailed error information at the moment.
#[allow(clippy::result_unit_err)]
pub fn replay_from_buf(buf: &[u8]) -> Result<(), ()> {
    // TODO: Instrument failures to find metrics by id.
    let ipc_payload: IPCPayload = bincode::deserialize(buf).map_err(|_| ())?;
    for (id, value) in ipc_payload.counters.into_iter() {
        if id.0 & (1 << crate::factory::DYNAMIC_METRIC_BIT) > 0 {
            let map = crate::factory::__jog_metric_maps::COUNTER_MAP
                .read()
                .expect("Read lock for dynamic counter map was poisoned");
            if let Some(metric) = map.get(&id) {
                metric.add(value);
            }
        } else if let Some(metric) = __glean_metric_maps::COUNTER_MAP.get(&id) {
            metric.add(value);
        }
    }
    for (id, samples) in ipc_payload.custom_samples.into_iter() {
        if id.0 & (1 << crate::factory::DYNAMIC_METRIC_BIT) > 0 {
            let map = crate::factory::__jog_metric_maps::CUSTOM_DISTRIBUTION_MAP
                .read()
                .expect("Read lock for dynamic custom distribution map was poisoned");
            if let Some(metric) = map.get(&id) {
                metric.accumulate_samples_signed(samples);
            }
        } else if let Some(metric) = __glean_metric_maps::CUSTOM_DISTRIBUTION_MAP.get(&id) {
            metric.accumulate_samples_signed(samples);
        }
    }
    for (id, value) in ipc_payload.denominators.into_iter() {
        if id.0 & (1 << crate::factory::DYNAMIC_METRIC_BIT) > 0 {
            let map = crate::factory::__jog_metric_maps::DENOMINATOR_MAP
                .read()
                .expect("Read lock for dynamic denominator map was poisoned");
            if let Some(metric) = map.get(&id) {
                metric.add(value);
            }
        } else if let Some(metric) = __glean_metric_maps::DENOMINATOR_MAP.get(&id) {
            metric.add(value);
        }
    }
    for (id, records) in ipc_payload.events.into_iter() {
        // TODO(bug XXX): Implement events in JOG
        for (timestamp, extra) in records.into_iter() {
            let _ = __glean_metric_maps::record_event_by_id_with_time(id, timestamp, extra);
        }
    }
    for (id, labeled_counts) in ipc_payload.labeled_counters.into_iter() {
        if id.0 & (1 << crate::factory::DYNAMIC_METRIC_BIT) > 0 {
            let map = crate::factory::__jog_metric_maps::LABELED_COUNTER_MAP
                .read()
                .expect("Read lock for dynamic labeled counter map was poisoned");
            if let Some(metric) = map.get(&id) {
                for (label, count) in labeled_counts.into_iter() {
                    metric.get(&label).add(count);
                }
            }
        } else if let Some(metric) = __glean_metric_maps::LABELED_COUNTER_MAP.get(&id) {
            for (label, count) in labeled_counts.into_iter() {
                metric.get(&label).add(count);
            }
        }
    }
    for (id, samples) in ipc_payload.memory_samples.into_iter() {
        if id.0 & (1 << crate::factory::DYNAMIC_METRIC_BIT) > 0 {
            let map = crate::factory::__jog_metric_maps::MEMORY_DISTRIBUTION_MAP
                .read()
                .expect("Read lock for dynamic memory dist map was poisoned");
            if let Some(metric) = map.get(&id) {
                samples
                    .into_iter()
                    .for_each(|sample| metric.accumulate(sample));
            }
        } else if let Some(metric) = __glean_metric_maps::MEMORY_DISTRIBUTION_MAP.get(&id) {
            samples
                .into_iter()
                .for_each(|sample| metric.accumulate(sample));
        }
    }
    for (id, value) in ipc_payload.numerators.into_iter() {
        if id.0 & (1 << crate::factory::DYNAMIC_METRIC_BIT) > 0 {
            let map = crate::factory::__jog_metric_maps::NUMERATOR_MAP
                .read()
                .expect("Read lock for dynamic numerator map was poisoned");
            if let Some(metric) = map.get(&id) {
                metric.add_to_numerator(value);
            }
        } else if let Some(metric) = __glean_metric_maps::NUMERATOR_MAP.get(&id) {
            metric.add_to_numerator(value);
        }
    }
    for (id, (n, d)) in ipc_payload.rates.into_iter() {
        if id.0 & (1 << crate::factory::DYNAMIC_METRIC_BIT) > 0 {
            let map = crate::factory::__jog_metric_maps::RATE_MAP
                .read()
                .expect("Read lock for dynamic rate map was poisoned");
            if let Some(metric) = map.get(&id) {
                metric.add_to_numerator(n);
                metric.add_to_denominator(d);
            }
        } else if let Some(metric) = __glean_metric_maps::RATE_MAP.get(&id) {
            metric.add_to_numerator(n);
            metric.add_to_denominator(d);
        }
    }
    for (id, strings) in ipc_payload.string_lists.into_iter() {
        if id.0 & (1 << crate::factory::DYNAMIC_METRIC_BIT) > 0 {
            let map = crate::factory::__jog_metric_maps::STRING_LIST_MAP
                .read()
                .expect("Read lock for dynamic string list map was poisoned");
            if let Some(metric) = map.get(&id) {
                strings.iter().for_each(|s| metric.add(s));
            }
        } else if let Some(metric) = __glean_metric_maps::STRING_LIST_MAP.get(&id) {
            strings.iter().for_each(|s| metric.add(s));
        }
    }
    for (id, samples) in ipc_payload.timing_samples.into_iter() {
        if id.0 & (1 << crate::factory::DYNAMIC_METRIC_BIT) > 0 {
            let map = crate::factory::__jog_metric_maps::TIMING_DISTRIBUTION_MAP
                .read()
                .expect("Read lock for dynamic timing distribution map was poisoned");
            if let Some(metric) = map.get(&id) {
                metric.accumulate_raw_samples_nanos(samples);
            }
        } else if let Some(metric) = __glean_metric_maps::TIMING_DISTRIBUTION_MAP.get(&id) {
            metric.accumulate_raw_samples_nanos(samples);
        }
    }
    Ok(())
}
