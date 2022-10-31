# [Android Components](../../../README.md) > Libraries > QR

A component that provides functionality for scanning QR coes.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:feature-qr:{latest-version}"
```

### Integration

Initializing the feature:

```kotlin
qrFeature = QrFeature(
    context,
    fragmentManager = supportFragmentManager,
    onNeedToRequestPermissions = { permissions ->
        requestPermissions(this, permissions, REQUEST_CODE_CAMERA_PERMISSIONS)
    },
    onScanResult = { result ->
        // result is a String (e.g. a URL) returned by the QR scanner.
    }
)
```

When ready to scan use the following:

```kotlin
qrFeature.scan()
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
