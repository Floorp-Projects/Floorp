# [Android Components](../../../README.md) > Feature > Service Worker

A component for adding support for all service workers' events and callbacks.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:feature-serviceworker:{latest-version}"
```

### Using it in an application

This needs to be installed as high up and as soon as possible, preferable in the `android.app.Application`.

```Kotlin
ServiceWorkerSupport.install(
    <Engine>,
    <TabsUseCases.AddNewTabUseCase>
)
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
