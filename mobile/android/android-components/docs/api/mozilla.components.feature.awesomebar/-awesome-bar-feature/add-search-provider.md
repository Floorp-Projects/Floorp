[android-components](../../index.md) / [mozilla.components.feature.awesomebar](../index.md) / [AwesomeBarFeature](index.md) / [addSearchProvider](./add-search-provider.md)

# addSearchProvider

`fun addSearchProvider(searchEngine: `[`SearchEngine`](../../mozilla.components.browser.search/-search-engine/index.md)`, searchUseCase: `[`SearchUseCase`](../../mozilla.components.feature.search/-search-use-cases/-search-use-case/index.md)`, fetchClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`, limit: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 15, mode: `[`Mode`](../../mozilla.components.feature.awesomebar.provider/-search-suggestion-provider/-mode/index.md)` = SearchSuggestionProvider.Mode.SINGLE_SUGGESTION, engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`? = null, filterExactMatch: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`AwesomeBarFeature`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/awesomebar/src/main/java/mozilla/components/feature/awesomebar/AwesomeBarFeature.kt#L83)

Adds a [AwesomeBar.SuggestionProvider](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/index.md) for search engine suggestions to the [AwesomeBar](../../mozilla.components.concept.awesomebar/-awesome-bar/index.md).

### Parameters

`searchEngine` - The search engine to request suggestions from.

`searchUseCase` - The use case to invoke for searches.

`fetchClient` - The HTTP client for requesting suggestions from the search engine.

`limit` - The maximum number of suggestions that should be returned. It needs to be &gt;= 1.

`mode` - Whether to return a single search suggestion (with chips) or one suggestion per item.

`engine` - optional [Engine](../../mozilla.components.concept.engine/-engine/index.md) instance to call [Engine.speculativeConnect](../../mozilla.components.concept.engine/-engine/speculative-connect.md) for the
highest scored search suggestion URL.

`filterExactMatch` - If true filters out suggestions that exactly match the entered text.`fun addSearchProvider(context: <ERROR CLASS>, searchEngineManager: `[`SearchEngineManager`](../../mozilla.components.browser.search/-search-engine-manager/index.md)`, searchUseCase: `[`SearchUseCase`](../../mozilla.components.feature.search/-search-use-cases/-search-use-case/index.md)`, fetchClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`, limit: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 15, mode: `[`Mode`](../../mozilla.components.feature.awesomebar.provider/-search-suggestion-provider/-mode/index.md)` = SearchSuggestionProvider.Mode.SINGLE_SUGGESTION, engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`? = null, filterExactMatch: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`AwesomeBarFeature`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/awesomebar/src/main/java/mozilla/components/feature/awesomebar/AwesomeBarFeature.kt#L121)

Adds a [AwesomeBar.SuggestionProvider](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/index.md) for search engine suggestions to the [AwesomeBar](../../mozilla.components.concept.awesomebar/-awesome-bar/index.md).
If the default search engine is to be used for fetching search engine suggestions then
this method is preferable over [addSearchProvider](./add-search-provider.md), as it will lazily load the default
search engine using the provided [SearchEngineManager](../../mozilla.components.browser.search/-search-engine-manager/index.md).

### Parameters

`context` - the activity or application context, required to load search engines.

`searchEngineManager` - The search engine manager to look up search engines.

`searchUseCase` - The use case to invoke for searches.

`fetchClient` - The HTTP client for requesting suggestions from the search engine.

`limit` - The maximum number of suggestions that should be returned. It needs to be &gt;= 1.

`mode` - Whether to return a single search suggestion (with chips) or one suggestion per item.

`engine` - optional [Engine](../../mozilla.components.concept.engine/-engine/index.md) instance to call [Engine.speculativeConnect](../../mozilla.components.concept.engine/-engine/speculative-connect.md) for the
highest scored search suggestion URL.

`filterExactMatch` - If true filters out suggestions that exactly match the entered text.