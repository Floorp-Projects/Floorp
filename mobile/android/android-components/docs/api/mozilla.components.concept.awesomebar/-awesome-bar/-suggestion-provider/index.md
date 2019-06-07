[android-components](../../../index.md) / [mozilla.components.concept.awesomebar](../../index.md) / [AwesomeBar](../index.md) / [SuggestionProvider](./index.md)

# SuggestionProvider

`interface SuggestionProvider` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/awesomebar/src/main/java/mozilla/components/concept/awesomebar/AwesomeBar.kt#L131)

A [SuggestionProvider](./index.md) is queried by an [AwesomeBar](../index.md) whenever the text in the address bar is changed by the user.
It returns a list of [Suggestion](../-suggestion/index.md)s to be displayed by the [AwesomeBar](../index.md).

### Properties

| Name | Summary |
|---|---|
| [id](id.md) | `abstract val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>A unique ID used for identifying this provider. |
| [shouldClearSuggestions](should-clear-suggestions.md) | `open val shouldClearSuggestions: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>If true an [AwesomeBar](../index.md) implementation can clear the previous suggestions of this provider as soon as the user continues to type. If this is false an [AwesomeBar](../index.md) implementation is allowed to keep the previous suggestions around until the provider returns a new list of suggestions for the updated text. |

### Functions

| Name | Summary |
|---|---|
| [onInputCancelled](on-input-cancelled.md) | `open fun onInputCancelled(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Fired when the user has cancelled their interaction with the awesome bar. |
| [onInputChanged](on-input-changed.md) | `abstract suspend fun onInputChanged(text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Suggestion`](../-suggestion/index.md)`>`<br>Fired whenever the user changes their input, after they have started interacting with the awesome bar. |
| [onInputStarted](on-input-started.md) | `open fun onInputStarted(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Suggestion`](../-suggestion/index.md)`>`<br>Fired when the user starts interacting with the awesome bar by entering text in the toolbar. |

### Inheritors

| Name | Summary |
|---|---|
| [BookmarksStorageSuggestionProvider](../../../mozilla.components.feature.awesomebar.provider/-bookmarks-storage-suggestion-provider/index.md) | `class BookmarksStorageSuggestionProvider : `[`SuggestionProvider`](./index.md)<br>A [AwesomeBar.SuggestionProvider](./index.md) implementation that provides suggestions based on the bookmarks stored in the [BookmarksStorage](../../../mozilla.components.concept.storage/-bookmarks-storage/index.md). |
| [ClipboardSuggestionProvider](../../../mozilla.components.feature.awesomebar.provider/-clipboard-suggestion-provider/index.md) | `class ClipboardSuggestionProvider : `[`SuggestionProvider`](./index.md)<br>An [AwesomeBar.SuggestionProvider](./index.md) implementation that returns a suggestions for an URL in the clipboard (if there's any). |
| [HistoryStorageSuggestionProvider](../../../mozilla.components.feature.awesomebar.provider/-history-storage-suggestion-provider/index.md) | `class HistoryStorageSuggestionProvider : `[`SuggestionProvider`](./index.md)<br>A [AwesomeBar.SuggestionProvider](./index.md) implementation that provides suggestions based on the browsing history stored in the [HistoryStorage](../../../mozilla.components.concept.storage/-history-storage/index.md). |
| [SearchSuggestionProvider](../../../mozilla.components.feature.awesomebar.provider/-search-suggestion-provider/index.md) | `class SearchSuggestionProvider : `[`SuggestionProvider`](./index.md)<br>A [AwesomeBar.SuggestionProvider](./index.md) implementation that provides a suggestion containing search engine suggestions (as chips) from the passed in [SearchEngine](../../../mozilla.components.browser.search/-search-engine/index.md). |
| [SessionSuggestionProvider](../../../mozilla.components.feature.awesomebar.provider/-session-suggestion-provider/index.md) | `class SessionSuggestionProvider : `[`SuggestionProvider`](./index.md)<br>A [AwesomeBar.SuggestionProvider](./index.md) implementation that provides suggestions based on the sessions in the [SessionManager](../../../mozilla.components.browser.session/-session-manager/index.md) (Open tabs). |
