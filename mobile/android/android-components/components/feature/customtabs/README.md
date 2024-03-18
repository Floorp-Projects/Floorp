# [Android Components](../../../README.md) > Feature > Custom Tabs

 A component for providing [Custom Tabs](https://developer.chrome.com/multidevice/android/customtabs) functionality in browsers.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:feature-customtabs:{latest-version}"
```

## Facts

This component emits the following [Facts](../../support/base/README.md#Facts):

| Action | Item              | Description                              |
|--------|-------------------|------------------------------------------|
| CLICK  | close             | The user clicked on the close button     |
| CLICK  | action_button     | The user clicked on an action button     |

In addition to the facts emitted above this feature will inject an addition extra (`customTab: true`) into the `BrowserMenuBuilder` passed to the `BrowserToolbar` (see [browser-menu](../../browser/menu/README.md)).

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
