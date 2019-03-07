# [Android Components](../../../README.md) > Service > Glean

A client-side telemetry SDK for collecting metrics and sending them to Mozilla's telemetry service
(eventually replacing [service-telemetry](../telemetry/README.md)).

- [Before using the library](#before-using-the-library)
- [Usage](#usage)
    - [Setting up the dependency](#setting-up-the-dependency)
    - [Integrating with the build system](#integrating-with-the-build-system)
    - [Initializing glean](#initializing-glean)
    - [Adding new metrics](#adding-new-metrics)
    - [Providing UI to enable / disable metrics](#providing-ui-to-enable--disable-metrics)
- [Debugging products using glean](#debugging-products-using-glean)
- [License](#license)

## Before using the library
Products using glean to collect telemetry **must**:

- add documentation for any new metric collected with the library in its repository (see [an example](https://github.com/mozilla-mobile/android-components/blob/df429df1a193516f796f2330863af384cce820bc/components/service/glean/docs/pings/pings.md));
- go through data review for the newly collected data by following [this process](https://wiki.mozilla.org/Firefox/Data_Collection);
- provide a way for users to turn data collection off (e.g. providing settings to control
  `Glean.setUploadEnabled()`).

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) 
([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:service-glean:{latest-version}"
```

### Integrating with the build system

In order for Glean to work, a Python environment must be accessible at build time. This is done
automatically by the [`com.jetbrains.python.envs`](https://github.com/JetBrains/gradle-python-envs/)
plugin. The plugin **must** be manually enabled by adding the following `plugins` block at the top
of the `build.gradle` file for your app module.

```Groovy
plugins {
    id "com.jetbrains.python.envs" version "0.0.26"
}
```

Right before the end of the same file, the glean build script must be included. This script can be
referenced directly from the GitHub repo, as shown below:

```Groovy
apply from: 'https://github.com/mozilla-mobile/android-components/raw/v{latest-version}/components/service/glean/scripts/sdk_generator.gradle'
```

**Important:** the `{latest-version}` placeholder in the above link should be replaced with the version
number of the glean library used by the project. For example, if version *0.34.2* is used, then the
include directive becomes:

```Groovy
apply from: 'https://github.com/mozilla-mobile/android-components/raw/v0.34.2/components/service/glean/scripts/sdk_generator.gradle'
```

### Initializing glean

Before any data collection can take place, glean **must** be initialized from the application. An
excellent place to perform this operation is within the `onCreate` method of the class that extends
Android's `Application` class.

```Kotlin
class SampleApplication : Application() {

    override fun onCreate() {
        super.onCreate()

        // Initialize the Glean library. Ideally, this is the first thing that
        // must be done right after enabling logging.
        Glean.initialize(applicationContext)
    }
}
```

Once initialized, if collection is enabled, glean will automatically start collecting [baseline metrics](metrics.yaml)
and sending its [pings](docs/pings/pings.md).

Glean should be initialized as soon as possible, and importantly, before any
other libraries in the application start using Glean. Library code should never
call `Glean.initialize`, since it should be called exactly once per application.

### Defining metrics

The metrics that your application collects must be defined in a `metrics.yaml`
file. This file should be at the root of the application module (the same
directory as the `build.gradle` file you updated). The format of that file is
documented [here](https://mozilla.github.io/glean_parser/metrics-yaml.html).

**Important**: as stated [here](#before-using-the-library), any new data collection requires
documentation and data-review. This is also required for any new metric automatically collected
by glean.

### Providing UI to enable / disable metrics

Every application must provide a way to disable and re-enable data collection and upload. This is
controlled with the `glean.setUploadEnabled()` method. The application should
provide some form of user interface to call this method.

## Debugging products using glean
Glean exports the [`GleanDebugActivity`](src/main/java/mozilla/components/service/glean/debug/GleanDebugActivity.kt)
that can be used to toggle debugging features on or off. Users can invoke this special activity, at
run-time, using the following [`adb`](https://developer.android.com/studio/command-line/adb) command:

`adb shell am start -n [package.name]/mozilla.components.service.glean.debug.GleanDebugActivity [extra keys]`

Where the `[package.name]` is the product's package name as exported in the manifest file (e.g.
`org.mozilla.samples.glean` for the glean sample application) and `[extra keys]` is a list of
extra keys to be passed to the debug activity. See the [documentation](https://developer.android.com/studio/command-line/adb#IntentSpec)
for the command line switches used to pass the extra keys. These are the currently supported keys:

|key|type|description|
|---|----|-----------|
| logPings | boolean (--ez) | If set to `true`, glean dumps pings to logcat; defaults to `false` |
| sendPing | string (--es) | Sends the ping with the given name immediately. |

For example, to direct the glean sample application to dump pings to logcat, and send the "metrics" ping immediately, the following command can be used:

```
adb shell am start -n org.mozilla.samples.glean/mozilla.components.service.glean.debug.GleanDebugActivity \
  --ez logPings true \
  --es sendPing metrics
```

## Contact

To contact us you can:
- Find us on the Mozilla Slack in *#glean*, on [Mozilla IRC](https://wiki.mozilla.org/IRC) in *#telemetry*.
- To report issues or request changes, file a bug in [Bugzilla in Toolkit::Telemetry](https://bugzilla.mozilla.org/enter_bug.cgi?product=Toolkit&component=Telemetry&status_whiteboard=%5Btelemetry%3Amobilesdk%3Am%3F%5D) and mention Glean in the title.
- Send an email to *glean-team@mozilla.com*.

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
