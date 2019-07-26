# [Android Components](../../../README.md) > Browser > Engine-Gecko

[*Engine*](../../concept/engine/README.md) implementation based on [GeckoView](https://wiki.mozilla.org/Mobile/GeckoView).

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:browser-engine-gecko-nightly:{latest-version}"
```

### Integration with the Glean SDK

The [Glean SDK](../../../components/service/glean/README.md) can be used to collect [Gecko Telemetry](https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/telemetry/index.html).
Applications using both this component and the Glean SDK should setup the Gecko Telemetry delegate
as shown below:

```Kotlin
    val builder = GeckoRuntimeSettings.Builder()
    val runtimeSettings = builder
        .telemetryDelegate(GeckoGleanAdapter()) // Sets up the delegate!
        .build()
    // Create the Gecko runtime.
    GeckoRuntime.create(context, runtimeSettings)
```

#### Adding new metrics

New Gecko metrics can be added as described [in the Firefox Telemetry docs](https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/telemetry/start/adding-a-new-probe.html).

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
