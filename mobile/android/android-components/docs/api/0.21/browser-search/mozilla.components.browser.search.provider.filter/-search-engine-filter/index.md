---
title: SearchEngineFilter - 
---

[mozilla.components.browser.search.provider.filter](../index.html) / [SearchEngineFilter](./index.html)

# SearchEngineFilter

`interface SearchEngineFilter`

Interface for classes that want to filter the list of search engines a SearchEngineProvider
implementation loads.

### Functions

| [filter](filter.html) | `abstract fun filter(context: Context, searchEngine: `[`SearchEngine`](../../mozilla.components.browser.search/-search-engine/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if the given search engine should be returned by the provider or false if this search engine should be ignored. |

