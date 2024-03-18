# [Android Components](../../../README.md) > Support > Webextensions

A component containing building blocks for features implemented as web extensions.

Usually this component never needs to be added to application projects manually. Other components may have a transitive dependency on some of the classes and interfaces in this component.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:support-webextensions:{latest-version}"
```

## Facts

This component emits the following [Facts](../../support/base/README.md#Facts):

| Action      | Item                       | Extras            | Description                     |
|-------------|----------------------------|-------------------|---------------------------------|
| Interaction | web_extensions_initialized | `extensionExtras` | Web extensions are initialized. |


#### `extensionExtras`

| Key         | Type         | Value                                 |
|-------------|--------------|---------------------------------------|
| "enabled"   | List<String> | List of enabled web extension ids.    |
| "installed" | List<String> | List of installed web extensions ids. |

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
