---
title: SearchEngineManager - 
---

[mozilla.components.browser.search](../index.html) / [SearchEngineManager](./index.html)

# SearchEngineManager

`class SearchEngineManager`

This class provides access to a centralized registry of search engines.

### Constructors

| [&lt;init&gt;](-init-.html) | `SearchEngineManager(providers: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`SearchEngineProvider`](../../mozilla.components.browser.search.provider/-search-engine-provider/index.html)`> = listOf(
            AssetsSearchEngineProvider(LocaleSearchLocalizationProvider())))`<br>This class provides access to a centralized registry of search engines. |

### Functions

| [getDefaultSearchEngine](get-default-search-engine.html) | `fun getDefaultSearchEngine(context: Context, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = EMPTY): `[`SearchEngine`](../-search-engine/index.html)<br>Returns the default search engine. |
| [getSearchEngines](get-search-engines.html) | `fun getSearchEngines(context: Context): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`SearchEngine`](../-search-engine/index.html)`>`<br>Returns all search engines. If no call to load() has been made then calling this method will perform a load. |
| [load](load.html) | `suspend fun load(context: Context): Deferred<`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`SearchEngine`](../-search-engine/index.html)`>>`<br>Asynchronously load search engines from providers. |
| [registerForLocaleUpdates](register-for-locale-updates.html) | `fun registerForLocaleUpdates(context: Context): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers for ACTION_LOCALE_CHANGED broadcasts and automatically reloads the search engines whenever the locale changes. |

