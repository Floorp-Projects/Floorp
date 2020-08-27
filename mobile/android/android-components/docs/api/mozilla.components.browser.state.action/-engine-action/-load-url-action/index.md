[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [EngineAction](../index.md) / [LoadUrlAction](./index.md)

# LoadUrlAction

`data class LoadUrlAction : `[`EngineAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L450)

Loads the given [url](url.md) in the tab with the given [sessionId](session-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `LoadUrlAction(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, flags: `[`LoadUrlFlags`](../../../mozilla.components.concept.engine/-engine-session/-load-url-flags/index.md)` = EngineSession.LoadUrlFlags.none(), additionalHeaders: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>? = null)`<br>Loads the given [url](url.md) in the tab with the given [sessionId](session-id.md). |

### Properties

| Name | Summary |
|---|---|
| [additionalHeaders](additional-headers.md) | `val additionalHeaders: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>?` |
| [flags](flags.md) | `val flags: `[`LoadUrlFlags`](../../../mozilla.components.concept.engine/-engine-session/-load-url-flags/index.md) |
| [sessionId](session-id.md) | `val sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [url](url.md) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
