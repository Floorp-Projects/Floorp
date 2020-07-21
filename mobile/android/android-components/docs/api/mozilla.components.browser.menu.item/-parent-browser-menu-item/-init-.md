[android-components](../../index.md) / [mozilla.components.browser.menu.item](../index.md) / [ParentBrowserMenuItem](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`ParentBrowserMenuItem(label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, @DrawableRes imageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, @ColorRes iconTintColorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = NO_ID, @ColorRes textColorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = NO_ID, subMenu: `[`BrowserMenu`](../../mozilla.components.browser.menu/-browser-menu/index.md)`, endOfMenuAlwaysVisible: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false)`

A parent menu item for displaying text and an image icon with a nested sub menu.
It handles back pressing if the sub menu contains a [BackPressMenuItem](../-back-press-menu-item/index.md).

### Parameters

`label` - The visible label of this menu item.

`imageResource` - ID of a drawable resource to be shown as icon.

`iconTintColorResource` - Optional ID of color resource to tint the icon.

`textColorResource` - Optional ID of color resource to tint the text.