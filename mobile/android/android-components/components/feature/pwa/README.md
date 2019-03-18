# [Android Components](../../../README.md) > Feature > Progressive Web Apps (PWA)

Feature implementation for Progressive Web Apps (PWA).

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:feature-pwa:{latest-version}"
```

### WebAppShellActivity

Standalone and fullscreen web apps are launched in a `WebAppShellActivity` instance. Since this instance requires access to some components and app using this component needs to create a new class and register it in the manifest:

```Kotlin
class WebAppActivity : AbstractWebAppShellActivity() {
    override val engine: Engine by lazy {
        /* Get Engine instance */
    }
    override val sessionManager: SessionManager by lazy {
        /* Get SessionManager instance */
    }
}
```

```xml
<activity android:name=".WebAppActivity">
    <intent-filter>
        <action android:name="mozilla.components.feature.pwa.SHELL" />
        <category android:name="android.intent.category.DEFAULT" />
    </intent-filter>
</activity>
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
