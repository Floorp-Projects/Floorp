[android-components](../../index.md) / [mozilla.components.browser.menu](../index.md) / [BrowserMenuBuilder](./index.md)

# BrowserMenuBuilder

`class BrowserMenuBuilder` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/BrowserMenuBuilder.kt#L15)

Helper class for building browser menus.

### Parameters

`items` - List of BrowserMenuItem objects to compose the menu from.

`extras` - Map of extra values that are added to emitted facts

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BrowserMenuBuilder(items: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`BrowserMenuItem`](../-browser-menu-item/index.md)`>, extras: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`> = emptyMap())`<br>Helper class for building browser menus. |

### Properties

| Name | Summary |
|---|---|
| [extras](extras.md) | `val extras: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`>`<br>Map of extra values that are added to emitted facts |
| [items](items.md) | `val items: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`BrowserMenuItem`](../-browser-menu-item/index.md)`>`<br>List of BrowserMenuItem objects to compose the menu from. |

### Functions

| Name | Summary |
|---|---|
| [build](build.md) | `fun build(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`): `[`BrowserMenu`](../-browser-menu/index.md) |
