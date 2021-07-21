# [Android Components](../../../README.md) > Browser > Menu

A generic menu with customizable items primarily for browser toolbars.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:browser-menu:{latest-version}"
```

### BrowserMenu
Sample code can be found in [Sample Toolbar app](https://github.com/mozilla-mobile/android-components/tree/main/samples/toolbar).

There are multiple properties that you customize of the menu browser by just adding them into your dimens.xml file.

```xml
<resources xmlns:tools="http://schemas.android.com/tools">

   <!--Change how rounded the corners of the menu should be-->
    <dimen name="mozac_browser_menu_corner_radius" tools:ignore="UnusedResources">4dp</dimen>

    <!--Change how much shadow the menu should have-->
    <dimen name="mozac_browser_menu_elevation" tools:ignore="UnusedResources">4dp</dimen>

    <!--Change the width of the menu-->
    <dimen name="mozac_browser_menu_width" tools:ignore="UnusedResources">250dp</dimen>

    <!--Change the dynamic width of the menu-->
    <dimen name="mozac_browser_menu_width_min" tools:ignore="UnusedResources">200dp</dimen>
    <dimen name="mozac_browser_menu_width_max" tools:ignore="UnusedResources">300dp</dimen>

    <!--Change the top and bottom padding of the menu-->
    <dimen name="mozac_browser_menu_padding_vertical" tools:ignore="UnusedResources">8dp</dimen>

</resources>
```
BrowserMenu can have a dynamic width:
- Using the same value for `mozac_browser_menu_width_min` and `mozac_browser_menu_width_max` means BrowserMenu will have a fixed width - `mozac_browser_menu_width`.
_This is the default behavior_.
- Different values for `mozac_browser_menu_width_min` and `mozac_browser_menu_width_max` means BrowserMenu will have a dynamic width depending on the widest BrowserMenuItem and between the aforementioned dimensions also taking into account display width.


### BrowserMenuDivider
```kotlin

    BrowserMenuDivider()

```

To customize the divider you could use a 1. Quick customization or a 2. Full customization:

1) If you just want to change the height of the divider, add this item your ``dimes.xml`` file, and your
prefer height size.

```xml
    <dimen name="mozac_browser_menu_item_divider_height" tools:ignore="UnusedResources">YOUR_HEIGHT</dimen>
```
2) For full customization, override the default style of the divider by adding this style item in your `style.xml` file, and customize to your liking.
```xml
        <style name="Mozac.Browser.Menu.Item.Divider.Horizontal" tools:ignore="UnusedResources">
            <item name="android:background">YOUR_BACKGROUND</item>
            <item name="android:layout_width">match_parent</item>
            <item name="android:layout_height">YOUR_HEIGHT</item>
        </style>
```

### BrowserMenuImageText
```kotlin
    BrowserMenuImageText(
       label = "Share",
       imageResource = R.drawable.mozac_ic_share,
       iconTintColorResource = R.color.photonBlue90
    ) {
        Toast.makeText(applicationContext, "Share", Toast.LENGTH_SHORT).show()
    }
```

To customize the menu you could use separate properties 1 or full access to the style of the menu 2:

1) If you just want to change a specify property, just add one these dimen items to your ``dimes.xml`` file.

```xml
    <!--Menu Item -->
       <!--Change the text_size for ALL menu items NOT only for the BrowserMenuImageText -->
        <dimen name="mozac_browser_menu_item_text_size" tools:ignore="UnusedResources">16sp</dimen>
    <!--Menu Item -->

    <!--Icon-->
       <!--Change the icon's width-->
        <dimen name="mozac_browser_menu_item_image_text_icon_width" tools:ignore="UnusedResources">24dp</dimen> <!--Default value-->

       <!--Change the icon's height-->
        <dimen name="mozac_browser_menu_item_image_text_icon_height" tools:ignore="UnusedResources">24dp</dimen> <!--Default value-->

    <!--Icon-->

    <!--Label-->
           <!--Change the separation between the label and the icon-->
            <dimen name="mozac_browser_menu_item_image_text_label_padding_start" tools:ignore="UnusedResources">20dp</dimen> <!--Default value-->

    <!--Label-->
```

2) For full customization, override the default style of menu by adding this style item in your `style.xml` file, and customize to your liking.

```xml
    <!--Change the appearance of all text menu items-->
    <style name="Mozac.Browser.Menu.Item.Text" parent="@android:style/TextAppearance.Material.Menu" tools:ignore="UnusedResources">
        <item name="android:background">?android:attr/selectableItemBackground</item>
        <item name="android:textSize">@dimen/mozac_browser_menu_item_text_size</item>
        <item name="android:ellipsize">end</item>
        <item name="android:lines">1</item>
        <item name="android:focusable">true</item>
        <item name="android:clickable">true</item>
    </style>

    <style name="Mozac.Browser.Menu.Item.ImageText.Icon" parent="" tools:ignore="UnusedResources">
        <item name="android:layout_width">@dimen/mozac_browser_menu_item_image_text_icon_width</item>
        <item name="android:layout_height">@dimen/mozac_browser_menu_item_image_text_icon_height</item>
    </style>

    <style name="Mozac.Browser.Menu.Item.ImageText.Label" parent="Mozac.Browser.Menu.Item.Text" tools:ignore="UnusedResources">
        <item name="android:layout_width">wrap_content</item>
        <item name="android:layout_height">wrap_content</item>
        <item name="android:paddingStart">@dimen/mozac_browser_menu_item_image_text_label_padding_start</item>
    </style>
```

## Facts

This component emits the following [Facts](../../support/base/README.md#Facts):

| Action | Item                    | Extras            | Description                          |
|--------|-------------------------|-------------------|--------------------------------------|
| Click  | web_extension_menu_item | `menuItemExtras`  | Web extension menu item was clicked. |


#### `menuItemExtras`

| Key  | Type   | Value                                                    |
|------|--------|----------------------------------------------------------|
| "id" | String | Web extension id of the clicked web extension menu item. |

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
