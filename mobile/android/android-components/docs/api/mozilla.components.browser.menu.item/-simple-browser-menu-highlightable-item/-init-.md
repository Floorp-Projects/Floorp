[android-components](../../index.md) / [mozilla.components.browser.menu.item](../index.md) / [SimpleBrowserMenuHighlightableItem](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`SimpleBrowserMenuHighlightableItem(label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, @ColorRes textColorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = NO_ID, textSize: `[`Float`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-float/index.html)` = NO_ID.toFloat(), backgroundTint: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, isHighlighted: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { true }, listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {})`

A menu item for displaying text with a highlight state which sets the
background of the menu item.

### Parameters

`label` - The default visible label of this menu item.

`textColorResource` - Optional ID of color resource to tint the text.

`textSize` - The size of the label.

`backgroundTint` - Tint for the menu item background color

`isHighlighted` - Whether or not to display the highlight

`listener` - Callback to be invoked when this menu item is clicked.