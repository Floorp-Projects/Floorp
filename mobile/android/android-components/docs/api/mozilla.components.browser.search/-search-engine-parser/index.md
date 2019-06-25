[android-components](../../index.md) / [mozilla.components.browser.search](../index.md) / [SearchEngineParser](./index.md)

# SearchEngineParser

`class SearchEngineParser` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/search/src/main/java/mozilla/components/browser/search/SearchEngineParser.kt#L25)

A very simple parser for search plugins.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SearchEngineParser()`<br>A very simple parser for search plugins. |

### Functions

| Name | Summary |
|---|---|
| [load](load.md) | `fun load(assetManager: <ERROR CLASS>, identifier: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`SearchEngine`](../-search-engine/index.md)<br>Loads a SearchEngine from the given path in assets and assigns it the given identifier.`fun load(identifier: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, stream: `[`InputStream`](https://developer.android.com/reference/java/io/InputStream.html)`): `[`SearchEngine`](../-search-engine/index.md)<br>Loads a SearchEngine from the given stream and assigns it the given identifier. |
