# [Android Components](../../../README.md) > Libraries > Fetch-HttpURLConnection

A [concept-fetch](../../concept/fetch/README.md) implementation using [HttpURLConnection](https://developer.android.com/reference/java/net/HttpURLConnection.html).

This implementation of `concept-fetch` uses [HttpURLConnection](https://developer.android.com/reference/java/net/HttpURLConnection.html) from the standard library of the Android System. Therefore this component has no third-party dependencies and is smaller than other implementations. It's intended use is for apps that have strict APK size constraints.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:lib-fetch-httpurlconnection:{latest-version}"
```

### Performing requests

See the [concept-fetch documentation](../../concept/fetch/README.md) for generic examples of using the API of components implementing `concept-fetch`.

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
