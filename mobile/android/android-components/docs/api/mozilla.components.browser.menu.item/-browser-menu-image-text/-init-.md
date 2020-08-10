[android-components](../../index.md) / [mozilla.components.browser.menu.item](../index.md) / [BrowserMenuImageText](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`BrowserMenuImageText(label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, @DrawableRes imageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, @ColorRes iconTintColorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = NO_ID, @ColorRes textColorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = NO_ID, listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {})`

A menu item for displaying text with an image icon.

### Parameters

`label` - The visible label of this menu item.

`imageResource` - ID of a drawable resource to be shown as icon.

`iconTintColorResource` - Optional ID of color resource to tint the icon.

`textColorResource` - Optional ID of color resource to tint the text.

`listener` - Callback to be invoked when this menu item is clicked.