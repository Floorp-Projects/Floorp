# [Android Components](../../../README.md) > Browser > Menu

A generic menu with customizable items primarily for browser toolbars.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:browser-menu:{latest-version}"
```

### BrowserMenu

There are multiple properties that you customize of the menu browser by just adding them into your dimens.xml file.

```xml
<resources xmlns:tools="http://schemas.android.com/tools">

   <!--Change how rounded the corners of the menu should be-->
    <dimen name="mozac_browser_menu_corner_radius">4dp</dimen>

    <!--Change how much shadow the menu should have-->
    <dimen name="mozac_browser_menu_elevation">4dp</dimen>

    <!--Change the width of the menu-->
    <dimen name="mozac_browser_menu_width">250dp</dimen>

    <!--Change the top and bottom padding of the menu-->
    <dimen name="mozac_browser_menu_padding_vertical">8dp</dimen>

</resources>
```

### BrowserMenuDivider
To customize the divider you can have a 1. Quick customization or a 2. Full customization:

1) If you just want to change the height of the divider, add this item your ``dimes.xml`` file, and your
prefer height size.

```xml
    <dimen name="mozac_browser_menu_item_divider_height">YOUR_HEIGHT</dimen>
```
2) For full customization, override the default style of the divider by adding this style item in your `style.xml` file, and customize to your liking.
```xml
        <style name="Mozac.Browser.Menu.Item.Divider.Horizontal">
            <item name="android:background">YOUR_BACKGROUND</item>
            <item name="android:layout_width">match_parent</item>
            <item name="android:layout_height">YOUR_HEIGHT</item>
        </style>
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
