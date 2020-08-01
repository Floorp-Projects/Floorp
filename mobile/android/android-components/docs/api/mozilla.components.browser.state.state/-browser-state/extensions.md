[android-components](../../index.md) / [mozilla.components.browser.state.state](../index.md) / [BrowserState](index.md) / [extensions](./extensions.md)

# extensions

`val extensions: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`WebExtensionState`](../-web-extension-state/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/state/BrowserState.kt#L29)

A map of extension IDs and web extensions of all installed web extensions.
The extensions here represent the default values for all [BrowserState.extensions](./extensions.md) and can
be overridden per [SessionState](../-session-state/index.md).

### Property

`extensions` - A map of extension IDs and web extensions of all installed web extensions.
The extensions here represent the default values for all [BrowserState.extensions](./extensions.md) and can
be overridden per [SessionState](../-session-state/index.md).