# [Android Components](../../../README.md) > Feature > WebCompat Reporter

A feature that enables users to report site issues to Mozilla's Web Compatibility team for further diagnosis.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:feature-webcompat-reporter:{latest-version}"
```

### Install WebExtension

To install the WebExtension, run

```kotlin
WebCompatReporterFeature.install(engine)
```

Please make sure to only run this once, as the feature itself does not check if the extension is already installed.

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
