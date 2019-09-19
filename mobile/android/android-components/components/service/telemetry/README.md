# [Android Components](../../../README.md) > Service > Telemetry

A generic library for sending telemetry pings from Android applications to Mozilla's telemetry service.

## Deprecated
This component is now deprecated: only maintenance fixes will be provided and no new feature development will happen.
Use [the Glean SDK](../glean) instead.
This library will not be removed until all projects using it start using the Glean SDK.
Please do reach out to the Glean team (Mozilla Slack in *#glean* or *glean-team@mozilla.com*) for support transitioning to the new SDK.

## Motivation

The goal of this library is to provide a generic set of components to support a variety of telemetry use cases. It tries to not be opinionated about dependency injection frameworks or http clients. The only dependency is ``support-annotations`` to ensure code quality.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:service-telemetry:{latest-version}"
```

### Debug

If you want to debug your Telemetry code, use ``DebugLogClient`` instead of ``HttpURLConnectionTelemetryClient``. And add ``Log.addSink(AndroidLogSink())`` before you check it in logcat. Beware, the tag you set in ``DebugLogClient`` won't be used. See [Logging](../../support/base/README.md#logging) section for more details.

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
