[android-components](../../index.md) / [mozilla.components.browser.menu](../index.md) / [WebExtensionBrowserMenuBuilder](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`WebExtensionBrowserMenuBuilder(items: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`BrowserMenuItem`](../-browser-menu-item/index.md)`>, extras: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`> = emptyMap(), endOfMenuAlwaysVisible: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, webExtIconTintColorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = NO_ID, onAddonsManagerTapped: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {}, appendExtensionSubMenuAtStart: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false)`

Browser menu builder with web extension support. It allows [WebExtensionBrowserMenu](../-web-extension-browser-menu/index.md) to add
web extension browser actions in a nested menu item. If there are no web extensions installed,
the web extension menu item would return an add-on manager menu item instead.

### Parameters

`store` - [BrowserStore](../../mozilla.components.browser.state.store/-browser-store/index.md) required to render web extension browser actions

`webExtIconTintColorResource` - Optional ID of color resource to tint the icons of back and
add-ons manager menu items.

`onAddonsManagerTapped` - Callback to be invoked when add-ons manager menu item is selected.

`appendExtensionSubMenuAtStart` - true if web extension sub menu appear at the top (start) of
the menu, false if web extensions appear at the bottom of the menu. Default to false (bottom).