# [Android Components](../../../README.md) > Service > Telemetry

A generic library for sending telemetry pings from Android applications to Mozilla's telemetry service.

## Motivation

The goal of this library is to provide a generic set of components to support a variety of telemetry use cases. It tries to not be opinionated about dependency injection frameworks or http clients. The only dependency is ``support-annotations`` to ensure code quality.

## Usage

### Setting up the dependency

Use gradle to download the library from JCenter:

```Groovy
implementation "org.mozilla.components:service-telemetry:{latest-version}"
```

### Debug

If you want to debug your Telemetry code, use ``DebugLogClient`` instead of ``HttpURLConnectionTelemetryClient``. And add ``Log.addSink(AndroidLogSink())`` before you check it in logcat. Beware, the tag you set in ``DebugLogClient`` won't be used. See [Logging](../../support/base/README.md#logging) section for more details.

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
