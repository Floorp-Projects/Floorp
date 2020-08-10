[android-components](../../index.md) / [mozilla.components.browser.menu.item](../index.md) / [BrowserMenuHighlightableItem](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`BrowserMenuHighlightableItem(label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, @DrawableRes imageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, @ColorRes iconTintColorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = NO_ID, @ColorRes textColorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = NO_ID, highlight: `[`Highlight`](-highlight/index.md)`? = null, listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {})``BrowserMenuHighlightableItem(label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, @DrawableRes startImageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, @ColorRes iconTintColorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = NO_ID, @ColorRes textColorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = NO_ID, highlight: `[`BrowserMenuHighlight`](../../mozilla.components.browser.menu/-browser-menu-highlight/index.md)`, isHighlighted: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { true }, listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {})`

A menu item for displaying text with an image icon and a highlight state which sets the
background of the menu item and a second image icon to the right of the text.

### Parameters

`label` - The default visible label of this menu item.

`startImageResource` - ID of a drawable resource to be shown as a leftmost icon.

`iconTintColorResource` - Optional ID of color resource to tint the icon.

`textColorResource` - Optional ID of color resource to tint the text.

`highlight` - Highlight object representing how the menu item will be displayed when highlighted.

`isHighlighted` - Whether or not to display the highlight

`listener` - Callback to be invoked when this menu item is clicked.