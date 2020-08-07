[android-components](../../index.md) / [mozilla.components.feature.awesomebar](../index.md) / [AwesomeBarFeature](index.md) / [addHistoryProvider](./add-history-provider.md)

# addHistoryProvider

`fun addHistoryProvider(historyStorage: `[`HistoryStorage`](../../mozilla.components.concept.storage/-history-storage/index.md)`, loadUrlUseCase: `[`LoadUrlUseCase`](../../mozilla.components.feature.session/-session-use-cases/-load-url-use-case/index.md)`, engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`? = null): `[`AwesomeBarFeature`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/awesomebar/src/main/java/mozilla/components/feature/awesomebar/AwesomeBarFeature.kt#L177)

Add a [AwesomeBar.SuggestionProvider](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/index.md) for browsing history to the [AwesomeBar](../../mozilla.components.concept.awesomebar/-awesome-bar/index.md).

### Parameters

`historyStorage` - and instance of the [BookmarksStorage](../../mozilla.components.concept.storage/-bookmarks-storage/index.md) used to query matching bookmarks.

`loadUrlUseCase` - the use case invoked to load the url when the user clicks on the suggestion.

`engine` - optional [Engine](../../mozilla.components.concept.engine/-engine/index.md) instance to call [Engine.speculativeConnect](../../mozilla.components.concept.engine/-engine/speculative-connect.md) for the
highest scored suggestion URL.