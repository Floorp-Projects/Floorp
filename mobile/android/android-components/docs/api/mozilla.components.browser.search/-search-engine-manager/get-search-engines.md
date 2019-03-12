[android-components](../../index.md) / [mozilla.components.browser.search](../index.md) / [SearchEngineManager](index.md) / [getSearchEngines](./get-search-engines.md)

# getSearchEngines

`@Synchronized fun getSearchEngines(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`SearchEngine`](../-search-engine/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/search/src/main/java/mozilla/components/browser/search/SearchEngineManager.kt#L50)

Returns all search engines. If no call to load() has been made then calling this method
will perform a load.

