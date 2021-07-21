# [Android Components](../../../README.md) > Browser > Menu 2

A generic menu with customizable items primarily for browser toolbars.

This replaces the [browser-menu](../menu) component with a new API using immutable objects,
designed to work well with [lib-state](../../lib/state).

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:browser-menu2:{latest-version}"
```

### MenuController
The menu controller is used to control the items in the menu as well as displaying the menu popup.

Sample code can be found in [Sample Toolbar app](https://github.com/mozilla-mobile/android-components/tree/main/samples/toolbar).

There are multiple properties that you customize of the browser menu by just adding them into your dimens.xml file.

```xml
<resources xmlns:tools="http://schemas.android.com/tools">

   <!--Change how rounded the corners of the menu should be-->
    <dimen name="mozac_browser_menu2_corner_radius" tools:ignore="UnusedResources">4dp</dimen>

    <!--Change how much shadow the menu should have-->
    <dimen name="mozac_browser_menu2_elevation" tools:ignore="UnusedResources">4dp</dimen>

    <!--Change the width of the menu - can also be set in MenuController#show()-->
    <dimen name="mozac_browser_menu2_width" tools:ignore="UnusedResources">250dp</dimen>

    <!--Change the top and bottom padding of the menu-->
    <dimen name="mozac_browser_menu2_padding_vertical" tools:ignore="UnusedResources">8dp</dimen>

</resources>
```

Options displayed in the menu are configured by using a list of `MenuCandidate` objects.
The list of options can be sent to the menu by calling `MenuController#submitList()`.
To change the displayed options, simply call `submitList` again with a new list.

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
