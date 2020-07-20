[android-components](../index.md) / [mozilla.components.browser.state.state](index.md) / [createTab](./create-tab.md)

# createTab

`fun createTab(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, private: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = UUID.randomUUID().toString(), parent: `[`TabSessionState`](-tab-session-state/index.md)`? = null, extensions: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`WebExtensionState`](-web-extension-state/index.md)`> = emptyMap(), readerState: `[`ReaderState`](-reader-state/index.md)` = ReaderState(), title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", thumbnail: <ERROR CLASS>? = null, contextId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, lastAccess: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = 0L, source: `[`Source`](-session-state/-source/index.md)` = SessionState.Source.NONE): `[`TabSessionState`](-tab-session-state/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/state/TabSessionState.kt#L59)

Convenient function for creating a tab.

