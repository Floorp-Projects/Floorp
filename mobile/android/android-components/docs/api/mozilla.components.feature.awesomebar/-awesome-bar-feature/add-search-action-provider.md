[android-components](../../index.md) / [mozilla.components.feature.awesomebar](../index.md) / [AwesomeBarFeature](index.md) / [addSearchActionProvider](./add-search-action-provider.md)

# addSearchActionProvider

`fun addSearchActionProvider(searchEngineGetter: suspend () -> `[`SearchEngine`](../../mozilla.components.browser.search/-search-engine/index.md)`, searchUseCase: `[`SearchUseCase`](../../mozilla.components.feature.search/-search-use-cases/-search-use-case/index.md)`, icon: <ERROR CLASS>? = null, showDescription: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`AwesomeBarFeature`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/awesomebar/src/main/java/mozilla/components/feature/awesomebar/AwesomeBarFeature.kt#L154)

Adds an [AwesomeBar.SuggestionProvider](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/index.md) implementation that always returns a suggestion that
mirrors the entered text and invokes a search with the given [SearchEngine](../../mozilla.components.browser.search/-search-engine/index.md) if clicked.

### Parameters

`searchEngine` - The search engine to search with.

`searchUseCase` - The use case to invoke for searches.

`icon` - The image to display next to the result. If not specified, the engine icon is used.

`showDescription` - whether or not to add the search engine name as description.