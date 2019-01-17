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
To customize the divider you could use a 1. Quick customization or a 2. Full customization:

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

### BrowserMenuImageText
To customize the menu you could use separate properties 1 or full access to the style of the menu 2:

1) If you just want to change a specify property, just add one these dimen items to your ``dimes.xml`` file.

```xml

    <!--Menu Item -->
       <!--Change the text_size for ALL menu items NOT only for the BrowserMenuImageText -->
        <dimen name="mozac_browser_menu_item_text_size">16sp</dimen>
    <!--Menu Item -->

    <!--Icon-->
       <!--Change the icon's width-->
        <dimen name="mozac_browser_menu_item_image_text_icon_width">24dp</dimen> <!--Default value-->

       <!--Change the icon's height-->
        <dimen name="mozac_browser_menu_item_image_text_icon_height">24dp</dimen> <!--Default value-->

    <!--Icon-->

    <!--Label-->
           <!--Change the label's layout_height-->
            <dimen name="mozac_browser_menu_item_image_text_label_layout_height">48dp</dimen> <!--Default value-->

           <!--Change the separation between the label and the icon-->
            <dimen name="mozac_browser_menu_item_image_text_label_padding_start">20dp</dimen> <!--Default value-->

    <!--Label-->

```

2) For full customization, override the default style of menu by adding this style item in your `style.xml` file, and customize to your liking.
```xml
    <style name="Mozac.Browser.Menu.Item.ImageText.Icon" parent="">
        <item name="android:layout_width">@dimen/mozac_browser_menu_item_image_text_icon_layout_width</item>
        <item name="android:layout_height">@dimen/mozac_browser_menu_item_image_text_icon_layout_height</item>
    </style>

    <style name="Mozac.Browser.Menu.Item.ImageText.Label" parent="@android:style/TextAppearance.Material.Menu">
        <item name="android:layout_width">wrap_content</item>
        <item name="android:layout_height">@dimen/mozac_browser_menu_item_image_text_label_layout_height</item>
        <item name="android:paddingStart">@dimen/mozac_browser_menu_item_image_text_label_padding_start</item>
        <item name="android:lines">1</item>
        <item name="android:textSize">@dimen/mozac_browser_menu_item_text_size</item>
        <item name="android:clickable">true</item>
        <item name="android:ellipsize">end</item>
        <item name="android:focusable">true</item>
    </style>
```
## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
