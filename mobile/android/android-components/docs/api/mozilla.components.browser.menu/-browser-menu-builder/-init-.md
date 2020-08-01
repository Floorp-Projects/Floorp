[android-components](../../index.md) / [mozilla.components.browser.menu](../index.md) / [BrowserMenuBuilder](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`BrowserMenuBuilder(items: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`BrowserMenuItem`](../-browser-menu-item/index.md)`>, extras: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`> = emptyMap(), endOfMenuAlwaysVisible: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false)`

Helper class for building browser menus.

### Parameters

`items` - List of BrowserMenuItem objects to compose the menu from.

`extras` - Map of extra values that are added to emitted facts

`endOfMenuAlwaysVisible` - when is set to true makes sure the bottom of the menu is always visible otherwise,
the top of the menu is always visible.