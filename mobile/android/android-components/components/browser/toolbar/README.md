# [Android Components](../../../README.md) > Browser > Toolbar

A customizable toolbar for browsers.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:browser-toolbar:{latest-version}"
```

## Facts

This component emits the following [Facts](../../support/base/README.md#Facts):

| Action | Item | Description |
|---|---|---|---|---|
| CLICK | menu | The user opened the overflow menu. |
| COMMIT | toolbar | The user has edited the URL. |

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
