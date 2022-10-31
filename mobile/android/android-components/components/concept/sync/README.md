# [Android Components](../../../README.md) > Concept > Sync

The `concept-sync` component contains interfaces and types that describe various aspects of data synchronization.

This abstraction makes it possible to create different implementations of synchronization backends, without tightly
coupling concrete implementations of storage, accounts and sync sub-systems.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:concept-sync:{latest-version}"
```

### Integration

TODO

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
