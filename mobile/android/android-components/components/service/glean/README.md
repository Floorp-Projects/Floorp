# [Android Components](../../../README.md) > Service > Glean

A client-side telemetry SDK for collecting metrics and sending them to Mozilla's telemetry service
(eventually replacing [service-telemetry](../telemetry/README.md)).

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
of the main `build.gradle` file.

```Groovy
plugins {
    id "com.jetbrains.python.envs" version "0.0.25"
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

Once initialized, glean will automatically start collecting and sending its [core metrics](metrics.yaml).

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
