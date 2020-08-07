[android-components](../../../index.md) / [mozilla.components.browser.engine.gecko](../../index.md) / [GeckoEngineSession](../index.md) / [LoadRequest](./index.md)

# LoadRequest

`data class LoadRequest` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/GeckoEngineSession.kt#L116)

Represents a request to load a [url](url.md).

### Parameters

`url` - the url to load.

`parent` - the parent (referring) [EngineSession](../../../mozilla.components.concept.engine/-engine-session/index.md) i.e. the session that
triggered creating this one.

`flags` - the [LoadUrlFlags](#) to use when loading the provided url.

`additionalHeaders` - the extra headers to use when loading the provided url.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `LoadRequest(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, parent: `[`EngineSession`](../../../mozilla.components.concept.engine/-engine-session/index.md)`?, flags: `[`LoadUrlFlags`](../../../mozilla.components.concept.engine/-engine-session/-load-url-flags/index.md)`, additionalHeaders: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>?)`<br>Represents a request to load a [url](url.md). |

### Properties

| Name | Summary |
|---|---|
| [additionalHeaders](additional-headers.md) | `val additionalHeaders: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>?`<br>the extra headers to use when loading the provided url. |
| [flags](flags.md) | `val flags: `[`LoadUrlFlags`](../../../mozilla.components.concept.engine/-engine-session/-load-url-flags/index.md)<br>the [LoadUrlFlags](#) to use when loading the provided url. |
| [parent](parent.md) | `val parent: `[`EngineSession`](../../../mozilla.components.concept.engine/-engine-session/index.md)`?`<br>the parent (referring) [EngineSession](../../../mozilla.components.concept.engine/-engine-session/index.md) i.e. the session that triggered creating this one. |
| [url](url.md) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the url to load. |
