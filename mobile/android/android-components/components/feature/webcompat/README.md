# [Android Components](../../../README.md) > Feature > WebCompat

Feature to enable website-hotfixing via the Web Compatibility System-Addon. More details are available at:

* [`github.com/mozilla/webcompat-addon`](https://github.com/mozilla/webcompat-addon), where the Web-Extension sources are tracked.
* [The Mozilla Wiki](https://wiki.mozilla.org/Compatibility/Go_Faster_Addon), where more information about the background of this extensions are available and our processes for authoring site patches are outlined.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:feature-webcompat:{latest-version}"
```

### Install WebExtension

To install the WebExtension, run

```kotlin
WebCompatFeature.install(engine)
```

Please make sure to only run this once, as the feature itself does not check if the extension is already installed.

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
