# [Android Components](../../../README.md) > Support > License

A component to help generate and display licensing agreements.

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:support-license:{latest-version}"
```

## Usage

### License Plugin

Add the license plugin directly to your **project's** `build.gradle` classpath:

```groovy
// root build.gradle
buildscript {
    dependencies {
        classpath "com.google.android.gms:oss-licenses-plugin:0.10.4"
    }
}
```

Apply the plugin to the top of your **application's** `build.gradle`:

```groovy
apply plugin: 'com.google.android.gms.oss-licenses-plugin'
```

### Display licenses

Extend the abstract `LibrariesListFragment` and provide the resource location to the licenses and the metadata:

```kotlin
class MyLicense : LibrariesListFragment() {
    override val resources = LicenseResources(
        licenses = R.raw.third_party_licenses,
        licenseMetadata = R.raw.third_party_license_metadata,
    )
}
```

Optionally, add more fragment decorations as desired. For example:

```kotlin
class MyLicense : LibrariesListFragment() {
    override fun onResume() {
        super.onResume()

        showToolbar("Licenses we use")
    }
}
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
