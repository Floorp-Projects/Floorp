[android-components](../../index.md) / [mozilla.components.browser.search](../index.md) / [SearchEngineManager](index.md) / [getDefaultSearchEngineAsync](./get-default-search-engine-async.md)

# getDefaultSearchEngineAsync

`suspend fun getDefaultSearchEngineAsync(context: <ERROR CLASS>, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = EMPTY): `[`SearchEngine`](../-search-engine/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/search/src/main/java/mozilla/components/browser/search/SearchEngineManager.kt#L129)

Returns the default search engine.

If defaultSearchEngine has not been set, the default engine is set by the search provider,
(e.g. as set in `list.json`). If that is not set, then the first search engine listed is
returned.

Optionally a name can be passed to this method (e.g. from the user's preferences). If
a matching search engine was loaded then this search engine will be returned instead.

