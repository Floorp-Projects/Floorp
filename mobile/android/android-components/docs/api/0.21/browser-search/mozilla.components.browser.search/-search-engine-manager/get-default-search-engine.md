---
title: SearchEngineManager.getDefaultSearchEngine - 
---

[mozilla.components.browser.search](../index.html) / [SearchEngineManager](index.html) / [getDefaultSearchEngine](./get-default-search-engine.html)

# getDefaultSearchEngine

`@Synchronized fun getDefaultSearchEngine(context: Context, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = EMPTY): `[`SearchEngine`](../-search-engine/index.html)

Returns the default search engine.

The default engine is the first engine loaded by the first provider passed to the
constructor of SearchEngineManager.

Optionally a name can be passed to this method (e.g. from the user's preferences). If
a matching search engine was loaded then this search engine will be returned instead.

