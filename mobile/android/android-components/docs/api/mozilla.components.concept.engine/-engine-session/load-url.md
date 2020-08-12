[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [EngineSession](index.md) / [loadUrl](./load-url.md)

# loadUrl

`abstract fun loadUrl(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, parent: `[`EngineSession`](index.md)`? = null, flags: `[`LoadUrlFlags`](-load-url-flags/index.md)` = LoadUrlFlags.none(), additionalHeaders: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineSession.kt#L449)

Loads the given URL.

### Parameters

`url` - the url to load.

`parent` - the parent (referring) [EngineSession](index.md) i.e. the session that
triggered creating this one.

`flags` - the [LoadUrlFlags](-load-url-flags/index.md) to use when loading the provided url.

`additionalHeaders` - the extra headers to use when loading the provided url.