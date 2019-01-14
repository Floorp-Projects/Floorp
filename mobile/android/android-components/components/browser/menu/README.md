# [Android Components](../../../README.md) > Browser > Menu

A generic menu with customizable items primarily for browser toolbars.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:browser-menu:{latest-version}"
```

### BrowserMenu

There are multiple properties that you customize of the menu browser
by just adding them into your dimens.xml file.

```xml
<resources xmlns:tools="http://schemas.android.com/tools">
    <dimen name="mozac_browser_menu_corner_radius">4dp</dimen>
    <dimen name="mozac_browser_menu_elevation">4dp</dimen>
    <dimen name="mozac_browser_menu_width">250dp</dimen>
    <dimen name="mozac_browser_menu_padding_vertical">8dp</dimen>
</resources>
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
