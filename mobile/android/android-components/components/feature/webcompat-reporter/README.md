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

### Providing the browser-XXX label for reports

The `install` function has an optional second parameter, `productName`. This allows reports to be labelled using the correct broswer-XXX label on webcompat.com. For example,

```
WebCompatReporterFeature.install(engine, "fenix")
```

would add the `browser-fenix` label to the report. Note that simply inventing new values here does not work, as each product name has to be safelisted by the WebCompat team on webcompat.com, so [please get in touch](https://wiki.mozilla.org/Compatibility#Core_Team) when you need to add a new product name.

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
