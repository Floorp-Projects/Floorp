# Glean SDK

The `Glean SDK` is a modern approach for a Telemetry library and is part of the [Glean project](https://docs.telemetry.mozilla.org/concepts/glean/glean.html).

## `glean-core`

This library provides the core functionality of the Glean SDK, including implementations of all metric types, the ping serializer and the storage layer.
It's used in all platform-specific wrappers.

It's not intended to be used by users directly.
Each supported platform has a specific Glean package with a nicer API.
A nice Rust API will be provided by the [Glean](https://crates.io/crates/glean) crate.

## Documentation

All documentation is available online:

* [The Glean SDK Book][book]
* [API documentation][apidocs]

[book]: https://mozilla.github.io/glean/
[apidocs]: https://mozilla.github.io/glean/docs/glean_core/index.html

## Usage

```rust
use glean_core::{Glean, Configuration, CommonMetricData, metrics::*};
let cfg = Configuration {
    data_path: "/tmp/glean".into(),
    application_id: "glean.sample.app".into(),
    upload_enabled: true,
    max_events: None,
};
let mut glean = Glean::new(cfg).unwrap();
let ping = PingType::new("sample", true, true, vec![]);
glean.register_ping_type(&ping);

let call_counter: CounterMetric = CounterMetric::new(CommonMetricData {
    name: "calls".into(),
    category: "local".into(),
    send_in_pings: vec!["sample".into()],
    ..Default::default()
});

call_counter.add(&glean, 1);

glean.submit_ping(&ping, None).unwrap();
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
