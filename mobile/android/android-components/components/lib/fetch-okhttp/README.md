# [Android Components](../../../README.md) > Libraries > Fetch-OkHttp

A [concept-fetch](../../concept/fetch/README.md) implementation using [OkHttp](https://github.com/square/okhttp).

This implementation of `concept-fetch` uses [OkHttp](https://github.com/square/okhttp) - a third-party library from Square. It is intended for apps that already use OkHttp and want components to use the same client.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:lib-fetch-okhttp:{latest-version}"
```

### Performing requests

See the [concept-fetch documentation](../../concept/fetch/README.md) for generic examples of using the API of components implementing `concept-fetch`.

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
