# [Android Components](../../../README.md) > Concept > Base

A component for basic interfaces needed by multiple components and that do not warrant a standalone component.

## Usage

Usually this component is not used by apps directly. Instead it will be referenced by other components as a transitive dependency.

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:concept-base:{latest-version}"
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
