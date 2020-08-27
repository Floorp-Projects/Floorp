[android-components](../../index.md) / [mozilla.components.browser.state.state](../index.md) / [TabSessionState](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`TabSessionState(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = UUID.randomUUID().toString(), content: `[`ContentState`](../-content-state/index.md)`, trackingProtection: `[`TrackingProtectionState`](../-tracking-protection-state/index.md)` = TrackingProtectionState(), engineState: `[`EngineState`](../-engine-state/index.md)` = EngineState(), extensionState: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`WebExtensionState`](../-web-extension-state/index.md)`> = emptyMap(), contextId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, source: `[`Source`](../-session-state/-source/index.md)` = SessionState.Source.NONE, crashed: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, parentId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, lastAccess: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = 0L, readerState: `[`ReaderState`](../-reader-state/index.md)` = ReaderState())`

Value type that represents the state of a tab (private or normal).

