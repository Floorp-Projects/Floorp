[android-components](../../index.md) / [mozilla.components.feature.awesomebar.provider](../index.md) / [SearchSuggestionProvider](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`SearchSuggestionProvider(searchEngine: `[`SearchEngine`](../../mozilla.components.browser.search/-search-engine/index.md)`, searchUseCase: `[`SearchUseCase`](../../mozilla.components.feature.search/-search-use-cases/-search-use-case/index.md)`, fetchClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`, limit: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 15, mode: `[`Mode`](-mode/index.md)` = Mode.SINGLE_SUGGESTION)`

A [AwesomeBar.SuggestionProvider](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/index.md) implementation that provides a suggestion containing search engine suggestions (as
chips) from the passed in [SearchEngine](../../mozilla.components.browser.search/-search-engine/index.md).

### Parameters

`searchEngine` - The search engine to request suggestions from.

`searchUseCase` - The use case to invoke for searches.

`fetchClient` - The HTTP client for requesting suggestions from the search engine.

`limit` - The maximum number of suggestions that should be returned. It needs to be &gt;= 1.

`mode` - Whether to return a single search suggestion (with chips) or one suggestion per item.