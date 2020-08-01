[android-components](../../index.md) / [mozilla.components.browser.menu](../index.md) / [WebExtensionBrowserMenuBuilder](./index.md)

# WebExtensionBrowserMenuBuilder

`class WebExtensionBrowserMenuBuilder : `[`BrowserMenuBuilder`](../-browser-menu-builder/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/WebExtensionBrowserMenuBuilder.kt#L29)

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

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WebExtensionBrowserMenuBuilder(items: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`BrowserMenuItem`](../-browser-menu-item/index.md)`>, extras: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`> = emptyMap(), endOfMenuAlwaysVisible: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, webExtIconTintColorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = NO_ID, onAddonsManagerTapped: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {}, appendExtensionSubMenuAtStart: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false)`<br>Browser menu builder with web extension support. It allows [WebExtensionBrowserMenu](../-web-extension-browser-menu/index.md) to add web extension browser actions in a nested menu item. If there are no web extensions installed, the web extension menu item would return an add-on manager menu item instead. |

### Inherited Properties

| Name | Summary |
|---|---|
| [endOfMenuAlwaysVisible](../-browser-menu-builder/end-of-menu-always-visible.md) | `val endOfMenuAlwaysVisible: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>when is set to true makes sure the bottom of the menu is always visible otherwise, the top of the menu is always visible. |
| [extras](../-browser-menu-builder/extras.md) | `val extras: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`>`<br>Map of extra values that are added to emitted facts |
| [items](../-browser-menu-builder/items.md) | `val items: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`BrowserMenuItem`](../-browser-menu-item/index.md)`>`<br>List of BrowserMenuItem objects to compose the menu from. |

### Functions

| Name | Summary |
|---|---|
| [build](build.md) | `fun build(context: <ERROR CLASS>): `[`BrowserMenu`](../-browser-menu/index.md)<br>Builds and returns a browser menu with combination of [items](../-browser-menu-builder/items.md) and web extension browser actions. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
