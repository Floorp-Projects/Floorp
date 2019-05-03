# [Android Components](../../../README.md) > Service > Glean

A client-side telemetry SDK for collecting metrics and sending them to Mozilla's telemetry service
(eventually replacing [service-telemetry](../telemetry/README.md)).

- [Before using the library](#before-using-the-library)
- [Usage](#usage)
    - [Setting up the dependency](#setting-up-the-dependency)
    - [Integrating with the build system](#integrating-with-the-build-system)
    - [Initializing Glean](#initializing-glean)
    - [Adding new metrics](#adding-new-metrics)
    - [Adding custom pings](#adding-custom-pings)
    - [Testing metrics](#testing-metrics)
    - [Providing UI to enable / disable metrics](#providing-ui-to-enable--disable-metrics)
- [Debugging products using Glean](#debugging-products-using-glean)
- [Data documentation](#data-documentation)
- [Contact](#contact)
- [License](#license)

## Before using the library
Products using Glean to collect telemetry **must**:

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

Right before the end of the same file, the Glean build script must be included. This script can be
referenced directly from the GitHub repo, as shown below:

```Groovy
apply from: 'https://github.com/mozilla-mobile/android-components/raw/v{latest-version}/components/service/glean/scripts/sdk_generator.gradle'
```

**Important:** the `{latest-version}` placeholder in the above link should be replaced with the version
number of the Glean library used by the project. For example, if version *0.34.2* is used, then the
include directive becomes:

```Groovy
apply from: 'https://github.com/mozilla-mobile/android-components/raw/v0.34.2/components/service/glean/scripts/sdk_generator.gradle'
```

### Initializing Glean

Before any data collection can take place, Glean **must** be initialized from the application. An
excellent place to perform this operation is within the `onCreate` method of the class that extends
Android's `Application` class.

```Kotlin
import org.mozilla.yourApplication.GleanMetrics.Pings

class SampleApplication : Application() {

    override fun onCreate() {
        super.onCreate()

        // If you have custom pings in your application, you must register them
        // using the following command. This command should be omitted for
        // applications not using custom pings.
        Glean.registerPings(Pings)

        // Call setUploadEnabled first, since Glean.initialize
        // might send pings if there are any metrics queued up
        // from a previous run.
        Glean.setUploadEnabled(Settings.isTelemetryEnabled)

        // Initialize the Glean library.
        Glean.initialize(applicationContext)
    }
}
```

Once initialized, if collection is enabled, Glean will automatically start collecting [baseline metrics](metrics.yaml)
and sending its [pings](docs/pings/pings.md).

Glean should be initialized as soon as possible, and importantly, before any
other libraries in the application start using Glean. Library code should never
call `Glean.initialize`, since it should be called exactly once per application.

**Note**: if the application has the concept of release channels and knows which channel it is on at
run-time, then it can provide Glean with this information by setting it as part of the `Configuration`
object parameter of the `Glean.initialize` method. For example:

```Kotlin
Glean.initialize(applicationContext, Configuration(channel = "beta"))
```

### Enabling and disabling metrics

`Glean.setUploadEnabled()` should be called in response to the user enabling or
disabling telemetry.  This method should also be called at least once prior
to calling `Glean.initialized()`.

When going from enabled to disabled, all pending events, metrics and pings are
cleared. When re-enabling, core Glean metrics will be recomputed at that time.

### Adding new metrics

All metrics that your application collects must be defined in a `metrics.yaml`
file. This file should be at the root of the application module (the same
directory as the `build.gradle` file you updated). The format of that file is
documented [here](https://mozilla.github.io/glean_parser/metrics-yaml.html).
To learn more, see [adding new metrics](docs/metrics/adding-new-metrics.md).

**Important**: as stated [here](#before-using-the-library), any new data collection requires
documentation and data-review. This is also required for any new metric automatically collected
by Glean.

### Adding custom pings

Please refer to the custom pings documentation living [here](docs/pings/custom.md).

**Important**: as stated [here](#before-using-the-library), any new data collection, including
new custom pings, requires documentation and data-review. This is also required for any new ping automatically collected by Glean.

### Testing metrics

In order to make testing metrics easier 'out of the box', all metrics include a set of test API 
functions in order to facilitate unit testing.  These include functions to test whether a value has
been stored, and functions to retrieve the stored value for validation.  For more information, 
please refer to [Unit testing Glean metrics](docs/metrics/testing-metrics.md).

### Providing UI to enable / disable metrics

Every application must provide a way to disable and re-enable data collection
and upload. This is controlled with the `glean.setUploadEnabled()` method. The
application should provide some form of user interface to call this method.
Additionally, it is good practice to call this method immediately before calling
`Glean.initialize()` to ensure that Glean doesn't send any pings at start up
when telemetry is disabled.

## Debugging products using Glean
Glean exports the [`GleanDebugActivity`](src/main/java/mozilla/components/service/glean/debug/GleanDebugActivity.kt)
that can be used to toggle debugging features on or off. Users can invoke this special activity, at
run-time, using the following [`adb`](https://developer.android.com/studio/command-line/adb) command:

`adb shell am start -n [applicationId]/mozilla.components.service.glean.debug.GleanDebugActivity [extra keys]`

In the above:

- `[applicationId]` is the product's application id as defined in the manifest
  file and/or build script. For the Glean sample application, this is
  `org.mozilla.samples.glean` for a release build and
  `org.mozilla.samples.glean.debug` for a debug build.

- `[extra keys]` is a list of extra keys to be passed to the debug activity. See the
  [documentation](https://developer.android.com/studio/command-line/adb#IntentSpec)
  for the command line switches used to pass the extra keys. These are the
  currently supported keys:

    |key|type|description|
    |---|----|-----------|
    | logPings | boolean (--ez) | If set to `true`, Glean dumps pings to logcat; defaults to `false` |
    | sendPing | string (--es) | Sends the ping with the given name immediately |
    | tagPings | string (--es) | Tags all outgoing pings as debug pings to make them available for real-time validation. The value must match the pattern `[a-zA-Z0-9-]{1,20}` |

For example, to direct a release build of the Glean sample application to (1) dump pings to logcat, (2) tag the ping 
with the `test-metrics-ping` tag, and (3) send the "metrics" ping immediately, the following command
can be used:

```
adb shell am start -n org.mozilla.samples.glean/mozilla.components.service.glean.debug.GleanDebugActivity \
  --ez logPings true \
  --es sendPing metrics \
  --es tagPings test-metrics-ping
```

### Important GleanDebugActivity notes!

- Options that are set using the adb flags are not immediately reset and will persist until the application is closed or manually reset.

- There are a couple different ways in which to send pings through the GleanDebugActivity.
    1. You can use the `GleanDebugActivity` in order to tag pings and trigger them manually using the UI.  This should always produce a ping with all required fields.
    2. You can use the `GleanDebugActivity` to tag _and_ send pings.  This has the side effect of potentially sending a ping which does not include all fields because `sendPings` triggers pings to be sent before certain application behaviors can occur which would record that information.  For example, `duration` is not calculated or included in a baseline ping sent with `sendPing` because it forces the ping to be sent before the `duration` metric has been recorded.

## Data documentation

Further documentation for pings that Glean can send out of the box is available [here](docs/pings/pings.md).

## Contact

To contact us you can:
- Find us on the Mozilla Slack in *#glean*, on [Mozilla IRC](https://wiki.mozilla.org/IRC) in *#telemetry*.
- To report issues or request changes, file a bug in [Bugzilla in Data Platform & Tools :: Glean: SDK](https://bugzilla.mozilla.org/enter_bug.cgi?product=Data%20Platform%20and%20Tools&component=Glean%3A%20SDK).
- Send an email to *glean-team@mozilla.com*.
- The Glean Android team is: *:dexter*, *:travis*, *:mdroettboom*, *:gfritzsche*

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
