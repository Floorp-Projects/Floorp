[android-components](../../index.md) / [mozilla.components.feature.awesomebar](../index.md) / [AwesomeBarFeature](index.md) / [addSearchProvider](./add-search-provider.md)

# addSearchProvider

`fun addSearchProvider(searchEngine: `[`SearchEngine`](../../mozilla.components.browser.search/-search-engine/index.md)`, searchUseCase: `[`SearchUseCase`](../../mozilla.components.feature.search/-search-use-cases/-search-use-case/index.md)`, fetchClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`, mode: `[`Mode`](../../mozilla.components.feature.awesomebar.provider/-search-suggestion-provider/-mode/index.md)` = SearchSuggestionProvider.Mode.SINGLE_SUGGESTION): `[`AwesomeBarFeature`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/awesomebar/src/main/java/mozilla/components/feature/awesomebar/AwesomeBarFeature.kt#L73)

Add a [AwesomeBar.SuggestionProvider](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/index.md) for search engine suggestions to the [AwesomeBar](../../mozilla.components.concept.awesomebar/-awesome-bar/index.md).

