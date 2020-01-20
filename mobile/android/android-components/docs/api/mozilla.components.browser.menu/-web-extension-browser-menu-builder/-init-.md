[android-components](../../index.md) / [mozilla.components.browser.menu](../index.md) / [WebExtensionBrowserMenuBuilder](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`WebExtensionBrowserMenuBuilder(items: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`BrowserMenuItem`](../-browser-menu-item/index.md)`>, extras: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`> = emptyMap(), endOfMenuAlwaysVisible: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, appendExtensionActionAtStart: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false)`

Browser menu builder with web extension support. It allows [WebExtensionBrowserMenu](../-web-extension-browser-menu/index.md) to add
web extension browser actions.

### Parameters

`store` - [BrowserStore](../../mozilla.components.browser.state.store/-browser-store/index.md) required to render web extension browser actions

`appendExtensionActionAtStart` - true if web extensions appear at the top (start) of the menu,
false if web extensions appear at the bottom of the menu. Default to false (bottom).