# [Android Components](../../../README.md) > Browser > Sync Storage

A syncable implementation of `concept-storage` backed by [application-services' Places lib](https://github.com/mozilla/application-services).

## Before using this component
Products sending telemetry and using this component *must request* a data-review following [this process](https://wiki.mozilla.org/Firefox/Data_Collection).
This component provides data collection using the [Glean SDK](https://mozilla.github.io/glean/book/index.html).
The list of metrics being collected is available in the [metrics documentation](../../support/sync-telemetry/docs/metrics.md).

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:browser-storage-sync:{latest-version}"
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
