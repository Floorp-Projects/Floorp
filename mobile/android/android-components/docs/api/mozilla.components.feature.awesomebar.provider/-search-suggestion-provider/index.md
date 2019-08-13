[android-components](../../index.md) / [mozilla.components.feature.awesomebar.provider](../index.md) / [SearchSuggestionProvider](./index.md)

# SearchSuggestionProvider

`class SearchSuggestionProvider : `[`SuggestionProvider`](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/awesomebar/src/main/java/mozilla/components/feature/awesomebar/provider/SearchSuggestionProvider.kt#L27)

A [AwesomeBar.SuggestionProvider](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/index.md) implementation that provides a suggestion containing search engine suggestions (as
chips) from the passed in [SearchEngine](../../mozilla.components.browser.search/-search-engine/index.md).

### Types

| Name | Summary |
|---|---|
| [Mode](-mode/index.md) | `enum class Mode` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SearchSuggestionProvider(searchEngine: `[`SearchEngine`](../../mozilla.components.browser.search/-search-engine/index.md)`, searchUseCase: `[`SearchUseCase`](../../mozilla.components.feature.search/-search-use-cases/-search-use-case/index.md)`, fetchClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`, limit: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 15, mode: `[`Mode`](-mode/index.md)` = Mode.SINGLE_SUGGESTION, icon: <ERROR CLASS>? = null)`<br>Creates a [SearchSuggestionProvider](./index.md) for the provided [SearchEngine](../../mozilla.components.browser.search/-search-engine/index.md).`SearchSuggestionProvider(context: <ERROR CLASS>, searchEngineManager: `[`SearchEngineManager`](../../mozilla.components.browser.search/-search-engine-manager/index.md)`, searchUseCase: `[`SearchUseCase`](../../mozilla.components.feature.search/-search-use-cases/-search-use-case/index.md)`, fetchClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`, limit: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 15, mode: `[`Mode`](-mode/index.md)` = Mode.SINGLE_SUGGESTION, icon: <ERROR CLASS>? = null)`<br>Creates a [SearchSuggestionProvider](./index.md) using the default engine as returned by the provided [SearchEngineManager](../../mozilla.components.browser.search/-search-engine-manager/index.md). |

### Properties

| Name | Summary |
|---|---|
| [id](id.md) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>A unique ID used for identifying this provider. |
| [shouldClearSuggestions](should-clear-suggestions.md) | `val shouldClearSuggestions: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>If true an [AwesomeBar](../../mozilla.components.concept.awesomebar/-awesome-bar/index.md) implementation can clear the previous suggestions of this provider as soon as the user continues to type. If this is false an [AwesomeBar](../../mozilla.components.concept.awesomebar/-awesome-bar/index.md) implementation is allowed to keep the previous suggestions around until the provider returns a new list of suggestions for the updated text. |

### Functions

| Name | Summary |
|---|---|
| [onInputChanged](on-input-changed.md) | `suspend fun onInputChanged(text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Suggestion`](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/index.md)`>`<br>Fired whenever the user changes their input, after they have started interacting with the awesome bar. |

### Inherited Functions

| Name | Summary |
|---|---|
| [onInputCancelled](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/on-input-cancelled.md) | `open fun onInputCancelled(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Fired when the user has cancelled their interaction with the awesome bar. |
| [onInputStarted](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/on-input-started.md) | `open fun onInputStarted(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Suggestion`](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/index.md)`>`<br>Fired when the user starts interacting with the awesome bar by entering text in the toolbar. |
