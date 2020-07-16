[android-components](../../index.md) / [mozilla.components.feature.awesomebar.provider](../index.md) / [SearchSuggestionProvider](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`SearchSuggestionProvider(searchEngine: `[`SearchEngine`](../../mozilla.components.browser.search/-search-engine/index.md)`, searchUseCase: `[`SearchUseCase`](../../mozilla.components.feature.search/-search-use-cases/-search-use-case/index.md)`, fetchClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`, limit: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 15, mode: `[`Mode`](-mode/index.md)` = Mode.SINGLE_SUGGESTION, engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`? = null, icon: <ERROR CLASS>? = null, showDescription: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, filterExactMatch: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false)`

Creates a [SearchSuggestionProvider](index.md) for the provided [SearchEngine](../../mozilla.components.browser.search/-search-engine/index.md).

### Parameters

`searchEngine` - The search engine to request suggestions from.

`searchUseCase` - The use case to invoke for searches.

`fetchClient` - The HTTP client for requesting suggestions from the search engine.

`limit` - The maximum number of suggestions that should be returned. It needs to be &gt;= 1.

`mode` - Whether to return a single search suggestion (with chips) or one suggestion per item.

`engine` - optional [Engine](../../mozilla.components.concept.engine/-engine/index.md) instance to call [Engine.speculativeConnect](../../mozilla.components.concept.engine/-engine/speculative-connect.md) for the
highest scored search suggestion URL.

`icon` - The image to display next to the result. If not specified, the engine icon is used.

`showDescription` - whether or not to add the search engine name as description.

`filterExactMatch` - If true filters out suggestions that exactly match the entered text.`SearchSuggestionProvider(context: <ERROR CLASS>, searchEngineManager: `[`SearchEngineManager`](../../mozilla.components.browser.search/-search-engine-manager/index.md)`, searchUseCase: `[`SearchUseCase`](../../mozilla.components.feature.search/-search-use-cases/-search-use-case/index.md)`, fetchClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`, limit: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 15, mode: `[`Mode`](-mode/index.md)` = Mode.SINGLE_SUGGESTION, engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`? = null, icon: <ERROR CLASS>? = null, showDescription: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, filterExactMatch: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false)`

Creates a [SearchSuggestionProvider](index.md) using the default engine as returned by the provided
[SearchEngineManager](../../mozilla.components.browser.search/-search-engine-manager/index.md).

### Parameters

`context` - the activity or application context, required to load search engines.

`searchEngineManager` - The search engine manager to look up search engines.

`searchUseCase` - The use case to invoke for searches.

`fetchClient` - The HTTP client for requesting suggestions from the search engine.

`limit` - The maximum number of suggestions that should be returned. It needs to be &gt;= 1.

`mode` - Whether to return a single search suggestion (with chips) or one suggestion per item.

`engine` - optional [Engine](../../mozilla.components.concept.engine/-engine/index.md) instance to call [Engine.speculativeConnect](../../mozilla.components.concept.engine/-engine/speculative-connect.md) for the
highest scored search suggestion URL.

`icon` - The image to display next to the result. If not specified, the engine icon is used.

`showDescription` - whether or not to add the search engine name as description.

`filterExactMatch` - If true filters out suggestions that exactly match the entered text.