---
title: SearchEngineManager.getSearchEngines - 
---

[mozilla.components.browser.search](../index.html) / [SearchEngineManager](index.html) / [getSearchEngines](./get-search-engines.html)

# getSearchEngines

`@Synchronized fun getSearchEngines(context: Context): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`SearchEngine`](../-search-engine/index.html)`>`

Returns all search engines. If no call to load() has been made then calling this method
will perform a load.

