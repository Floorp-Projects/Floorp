# glean-preview

The `Glean SDK` is a modern approach for a Telemetry library and is part of the [Glean project](https://docs.telemetry.mozilla.org/concepts/glean/glean.html).

## `glean-preview`


This library provides a Rust API on top of Glean, targeted to Rust consumers.

**Note: `glean-preview` is currently under development and not yet ready for use.**

## Documentation

All documentation is available online:

* [The Glean SDK Book][book]
* [API documentation][apidocs]

[book]: https://mozilla.github.io/glean/
[apidocs]: https://mozilla.github.io/glean/docs/glean_preview/index.html

## Example

```rust,no_run
use glean_preview::{Configuration, Error, metrics::*};

let cfg = Configuration {
    data_path: "/tmp/data".into(),
    application_id: "org.mozilla.glean_core.example".into(),
    upload_enabled: true,
    max_events: None,
    delay_ping_lifetime_io: false,
};
glean_preview::initialize(cfg)?;

let prototype_ping = PingType::new("prototype", true, true);

glean_preview::register_ping_type(&prototype_ping);

prototype_ping.submit();
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
