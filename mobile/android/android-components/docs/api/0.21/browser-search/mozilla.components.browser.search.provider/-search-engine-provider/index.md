---
title: SearchEngineProvider - 
---

[mozilla.components.browser.search.provider](../index.html) / [SearchEngineProvider](./index.html)

# SearchEngineProvider

`interface SearchEngineProvider`

Interface for classes that load search engines from a specific source.

### Functions

| [loadSearchEngines](load-search-engines.html) | `abstract suspend fun loadSearchEngines(context: Context): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`SearchEngine`](../../mozilla.components.browser.search/-search-engine/index.html)`>`<br>Load search engines from this provider. |

### Inheritors

| [AssetsSearchEngineProvider](../-assets-search-engine-provider/index.html) | `class AssetsSearchEngineProvider : `[`SearchEngineProvider`](./index.md)<br>SearchEngineProvider implementation to load the included search engines from assets. |

