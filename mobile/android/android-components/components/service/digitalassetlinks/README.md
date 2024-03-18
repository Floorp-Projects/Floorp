# [Android Components](../../../README.md) > Service > Digital Asset Links

A library for communicating with the [Digital Asset Links](https://developers.google.com/digital-asset-links) API.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/)
([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:service-digital-asset-links:{latest-version}"
```

### Obtaining an AssetDescriptor

For web sites, asset descriptors can be obtained by simply passing the origin into the `AssetDescriptor.Web` constructor.

```kotlin
AssetDescriptor.Web(
  site = "https://{fully-qualified domain}{:optional port}"
)
```

For Android apps, a fingerprint corresponding to the Android app must be used. This can be obtained using the `AndroidAssetFinder` class.

### Remote API

The `DigitalAssetLinksApi` class will handle checking asset links by calling [Google's remote API](https://developers.google.com/digital-asset-links/reference/rest). An API key must be given to the class.

### Local API

The `StatementRelationChecker` class will handle checking asset links on device by fetching and iterating through asset link statements located on a website. Either the `StatementApi` or `DigitalAssetLinksApi` classes may be used to obtain a statement list.

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
