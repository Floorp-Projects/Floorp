// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! This integration test should model how the RLB is used when embedded in another Rust application
//! (e.g. FOG/Firefox Desktop).
//!
//! We write a single test scenario per file to avoid any state keeping across runs
//! (different files run as different processes).

mod common;

/// Some user metrics.
mod metrics {
    use glean::private::*;
    use glean::{Lifetime, TimeUnit};
    use glean_core::CommonMetricData;
    use once_cell::sync::Lazy;

    #[allow(non_upper_case_globals)]
    pub static initialization: Lazy<TimespanMetric> = Lazy::new(|| {
        TimespanMetric::new(
            CommonMetricData {
                name: "initialization".into(),
                category: "sample".into(),
                send_in_pings: vec!["validation".into()],
                lifetime: Lifetime::Ping,
                disabled: false,
                ..Default::default()
            },
            TimeUnit::Nanosecond,
        )
    });
}

mod pings {
    use glean::private::PingType;
    use once_cell::sync::Lazy;

    #[allow(non_upper_case_globals)]
    pub static validation: Lazy<PingType> =
        Lazy::new(|| glean::private::PingType::new("validation", true, true, true, vec![]));
}

/// Test scenario: Glean is never initialized.
///
/// Glean is never initialized.
/// Some data is recorded early on.
/// And later the whole process is shutdown.
#[test]
fn never_initialize() {
    common::enable_test_logging();

    metrics::initialization.start();

    // NOT calling `initialize` here.
    // In apps this might happen for several reasons:
    // 1. Process doesn't run long enough for Glean to be initialized.
    // 2. Getting some early data used for initialize fails

    pings::validation.submit(None);

    // We can't test for data either, as that would panic because init was never called.

    glean::shutdown();
}
