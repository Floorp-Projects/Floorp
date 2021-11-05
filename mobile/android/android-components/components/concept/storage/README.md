# [Android Components](../../../README.md) > Concept > Storage

The `concept-storage` component contains interfaces and abstract classes that describe a "core data" storage layer.

This abstraction makes it possible to build components that work independently of the storage layer being used.

Currently a single store implementation is available:
- [syncable, Rust Places storage](../../browser/storage-sync) - compatible with the Firefox Sync ecosystem

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:concept-storage:{latest-version}"
```

### Integration

One way to interact with a `concept-storage` component is via [feature-storage](../../features/storage/README.md), which provides "glue" implementations that make use of storage. For example, a `features.storage.HistoryTrackingFeature` allows a `concept.engine.Engine` to keep track of visits and page meta information.

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
