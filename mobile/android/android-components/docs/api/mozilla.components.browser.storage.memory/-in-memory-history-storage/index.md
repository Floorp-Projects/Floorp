[android-components](../../index.md) / [mozilla.components.browser.storage.memory](../index.md) / [InMemoryHistoryStorage](./index.md)

# InMemoryHistoryStorage

`class InMemoryHistoryStorage : `[`HistoryStorage`](../../mozilla.components.concept.storage/-history-storage/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-memory/src/main/java/mozilla/components/browser/storage/memory/InMemoryHistoryStorage.kt#L23)

An in-memory implementation of [mozilla.components.concept.storage.HistoryStorage](../../mozilla.components.concept.storage/-history-storage/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `InMemoryHistoryStorage()`<br>An in-memory implementation of [mozilla.components.concept.storage.HistoryStorage](../../mozilla.components.concept.storage/-history-storage/index.md). |

### Functions

| Name | Summary |
|---|---|
| [cleanup](cleanup.md) | `fun cleanup(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Cleanup any allocated resources. |
| [getAutocompleteSuggestion](get-autocomplete-suggestion.md) | `fun getAutocompleteSuggestion(query: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`HistoryAutocompleteResult`](../../mozilla.components.concept.storage/-history-autocomplete-result/index.md)`?`<br>Retrieves domain suggestions which best match the [query](../../mozilla.components.concept.storage/-history-storage/get-autocomplete-suggestion.md#mozilla.components.concept.storage.HistoryStorage$getAutocompleteSuggestion(kotlin.String)/query). |
| [getDetailedVisits](get-detailed-visits.md) | `suspend fun getDetailedVisits(start: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, end: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`VisitInfo`](../../mozilla.components.concept.storage/-visit-info/index.md)`>`<br>Retrieves detailed information about all visits that occurred in the given time range. |
| [getSuggestions](get-suggestions.md) | `fun getSuggestions(query: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, limit: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`SearchResult`](../../mozilla.components.concept.storage/-search-result/index.md)`>`<br>Retrieves suggestions matching the [query](../../mozilla.components.concept.storage/-history-storage/get-suggestions.md#mozilla.components.concept.storage.HistoryStorage$getSuggestions(kotlin.String, kotlin.Int)/query). |
| [getVisited](get-visited.md) | `suspend fun getVisited(uris: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>`<br>Maps a list of page URIs to a list of booleans indicating if each URI was visited.`suspend fun getVisited(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>Retrieves a list of all visited pages. |
| [recordObservation](record-observation.md) | `suspend fun recordObservation(uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, observation: `[`PageObservation`](../../mozilla.components.concept.storage/-page-observation/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Records an observation about a page. |
| [recordVisit](record-visit.md) | `suspend fun recordVisit(uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, visitType: `[`VisitType`](../../mozilla.components.concept.storage/-visit-type/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Records a visit to a page. |
