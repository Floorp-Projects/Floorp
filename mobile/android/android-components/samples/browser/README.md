# [Android Components](../../README.md) > Samples > Browser

![](src/main/res/mipmap-xhdpi/ic_launcher.png)

A simple browser app that is composed from the browser components in this repository.

⚠️ **Note**: This sample application is only a very basic browser. For a full-featured reference browser implementation see the **[reference-browser repository](https://github.com/mozilla-mobile/reference-browser)**.

## Build variants

The browser app uses a product flavor:

* **channel**: Using different release channels of GeckoView: _nightly_, _beta_, _production_. In most cases you want to use the _nightly_ flavor as this will support all of the latest functionality.

## Glean SDK support

This sample application comes with Glean SDK telemetry initialized by default, but with upload disabled (no data is being sent).
This is for creating a simpler metric testing workflow for Gecko engineers that need to add their metrics to Gecko and expose them to Mozilla mobile products.
See [this bug](https://bugzilla.mozilla.org/show_bug.cgi?id=1592935) for more context.

In order to enable data upload for testing purposes, change the `Glean.setUploadEnabled(false)` to `Glean.setUploadEnabled(true)` in [`SampleApplication.kt`](src/main/java/org/mozilla/samples/browser/SampleApplication.kt).

Glean will send metrics from any Glean-enabled component used in this sample application:

- [engine-gecko-nightly](https://github.com/mozilla-mobile/android-components/blob/main/components/browser/engine-gecko-nightly/docs/metrics.md);
- [engine-gecko-beta](https://github.com/mozilla-mobile/android-components/blob/main/components/browser/engine-gecko-beta/docs/metrics.md);
- [engine-gecko](https://github.com/mozilla-mobile/android-components/blob/main/components/browser/engine-gecko/docs/metrics.md);

Data review for enabling the Glean SDK for this application can be found [here](https://bugzilla.mozilla.org/show_bug.cgi?id=1592935#c6).

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
