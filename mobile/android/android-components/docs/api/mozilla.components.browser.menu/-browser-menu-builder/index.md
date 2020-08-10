[android-components](../../index.md) / [mozilla.components.browser.menu](../index.md) / [BrowserMenuBuilder](./index.md)

# BrowserMenuBuilder

`open class BrowserMenuBuilder` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/BrowserMenuBuilder.kt#L17)

Helper class for building browser menus.

### Parameters

`items` - List of BrowserMenuItem objects to compose the menu from.

`extras` - Map of extra values that are added to emitted facts

`endOfMenuAlwaysVisible` - when is set to true makes sure the bottom of the menu is always visible otherwise,
the top of the menu is always visible.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BrowserMenuBuilder(items: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`BrowserMenuItem`](../-browser-menu-item/index.md)`>, extras: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`> = emptyMap(), endOfMenuAlwaysVisible: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false)`<br>Helper class for building browser menus. |

### Properties

| Name | Summary |
|---|---|
| [endOfMenuAlwaysVisible](end-of-menu-always-visible.md) | `val endOfMenuAlwaysVisible: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>when is set to true makes sure the bottom of the menu is always visible otherwise, the top of the menu is always visible. |
| [extras](extras.md) | `val extras: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`>`<br>Map of extra values that are added to emitted facts |
| [items](items.md) | `val items: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`BrowserMenuItem`](../-browser-menu-item/index.md)`>`<br>List of BrowserMenuItem objects to compose the menu from. |

### Functions

| Name | Summary |
|---|---|
| [build](build.md) | `open fun build(context: <ERROR CLASS>): `[`BrowserMenu`](../-browser-menu/index.md)<br>Builds and returns a browser menu with [items](items.md) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [WebExtensionBrowserMenuBuilder](../-web-extension-browser-menu-builder/index.md) | `class WebExtensionBrowserMenuBuilder : `[`BrowserMenuBuilder`](./index.md)<br>Browser menu builder with web extension support. It allows [WebExtensionBrowserMenu](../-web-extension-browser-menu/index.md) to add web extension browser actions in a nested menu item. If there are no web extensions installed, the web extension menu item would return an add-on manager menu item instead. |
