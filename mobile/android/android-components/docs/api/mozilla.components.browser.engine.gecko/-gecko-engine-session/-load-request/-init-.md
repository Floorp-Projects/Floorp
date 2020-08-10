[android-components](../../../index.md) / [mozilla.components.browser.engine.gecko](../../index.md) / [GeckoEngineSession](../index.md) / [LoadRequest](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`LoadRequest(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, parent: `[`EngineSession`](../../../mozilla.components.concept.engine/-engine-session/index.md)`?, flags: `[`LoadUrlFlags`](../../../mozilla.components.concept.engine/-engine-session/-load-url-flags/index.md)`, additionalHeaders: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>?)`

Represents a request to load a [url](url.md).

### Parameters

`url` - the url to load.

`parent` - the parent (referring) [EngineSession](../../../mozilla.components.concept.engine/-engine-session/index.md) i.e. the session that
triggered creating this one.

`flags` - the [LoadUrlFlags](#) to use when loading the provided url.

`additionalHeaders` - the extra headers to use when loading the provided url.