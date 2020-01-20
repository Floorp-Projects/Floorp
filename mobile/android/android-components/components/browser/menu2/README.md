# [Android Components](../../../README.md) > Browser > Menu 2

A generic menu with customizable items primarily for browser toolbars.

This replaces the [browser-menu](../menu) component.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:browser-menu2:{latest-version}"
```

### MenuController
Sample code can be found in [Sample Toolbar app](https://github.com/mozilla-mobile/android-components/tree/master/samples/toolbar).

There are multiple properties that you customize of the menu browser by just adding them into your dimens.xml file.

```xml
<resources xmlns:tools="http://schemas.android.com/tools">

   <!--Change how rounded the corners of the menu should be-->
    <dimen name="mozac_browser_menu_corner_radius" tools:ignore="UnusedResources">4dp</dimen>

    <!--Change how much shadow the menu should have-->
    <dimen name="mozac_browser_menu_elevation" tools:ignore="UnusedResources">4dp</dimen>

    <!--Change the width of the menu-->
    <dimen name="mozac_browser_menu_width" tools:ignore="UnusedResources">250dp</dimen>

    <!--Change the top and bottom padding of the menu-->
    <dimen name="mozac_browser_menu_padding_vertical" tools:ignore="UnusedResources">8dp</dimen>

</resources>
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
