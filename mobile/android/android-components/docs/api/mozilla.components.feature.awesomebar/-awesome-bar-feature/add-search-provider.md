[android-components](../../index.md) / [mozilla.components.feature.awesomebar](../index.md) / [AwesomeBarFeature](index.md) / [addSearchProvider](./add-search-provider.md)

# addSearchProvider

`fun addSearchProvider(searchEngine: `[`SearchEngine`](../../mozilla.components.browser.search/-search-engine/index.md)`, searchUseCase: `[`SearchUseCase`](../../mozilla.components.feature.search/-search-use-cases/-search-use-case/index.md)`, fetchClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`, limit: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 15, mode: `[`Mode`](../../mozilla.components.feature.awesomebar.provider/-search-suggestion-provider/-mode/index.md)` = SearchSuggestionProvider.Mode.SINGLE_SUGGESTION): `[`AwesomeBarFeature`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/awesomebar/src/main/java/mozilla/components/feature/awesomebar/AwesomeBarFeature.kt#L64)

Adds a [AwesomeBar.SuggestionProvider](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/index.md) for search engine suggestions to the [AwesomeBar](../../mozilla.components.concept.awesomebar/-awesome-bar/index.md).

`fun addSearchProvider(context: <ERROR CLASS>, searchEngineManager: `[`SearchEngineManager`](../../mozilla.components.browser.search/-search-engine-manager/index.md)`, searchUseCase: `[`SearchUseCase`](../../mozilla.components.feature.search/-search-use-cases/-search-use-case/index.md)`, fetchClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`, limit: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 15, mode: `[`Mode`](../../mozilla.components.feature.awesomebar.provider/-search-suggestion-provider/-mode/index.md)` = SearchSuggestionProvider.Mode.SINGLE_SUGGESTION): `[`AwesomeBarFeature`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/awesomebar/src/main/java/mozilla/components/feature/awesomebar/AwesomeBarFeature.kt#L82)

Adds a [AwesomeBar.SuggestionProvider](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/index.md) for search engine suggestions to the [AwesomeBar](../../mozilla.components.concept.awesomebar/-awesome-bar/index.md).
If the default search engine is to be used for fetching search engine suggestions then
this method is preferable over [addSearchProvider](./add-search-provider.md), as it will lazily load the default
search engine using the provided [SearchEngineManager](../../mozilla.components.browser.search/-search-engine-manager/index.md).

