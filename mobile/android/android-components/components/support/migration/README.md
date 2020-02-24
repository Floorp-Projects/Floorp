# [Android Components](../../../README.md) > Support > Migration

Helper code to migrate from a Fennec-based (Firefox for Android) app to an Android Components based app.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:support-migration:{latest-version}"
```

## Facts

This component emits the following [Facts](../../support/migration/README.md#Facts):

| Action       | Item                | Description              |
|--------------|---------------------|--------------------------|
| Interaction  | migration_started   | Migration was started.   |
| Interaction  | migration_completed | Migration was completed. |

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
