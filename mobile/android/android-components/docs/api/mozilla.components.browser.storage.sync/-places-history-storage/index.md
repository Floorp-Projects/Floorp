[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesHistoryStorage](./index.md)

# PlacesHistoryStorage

`open class PlacesHistoryStorage : `[`PlacesStorage`](../-places-storage/index.md)`, `[`HistoryStorage`](../../mozilla.components.concept.storage/-history-storage/index.md)`, `[`SyncableStore`](../../mozilla.components.concept.sync/-syncable-store/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesHistoryStorage.kt#L27)

Implementation of the [HistoryStorage](../../mozilla.components.concept.storage/-history-storage/index.md) which is backed by a Rust Places lib via [PlacesApi](#).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `PlacesHistoryStorage(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`)`<br>Implementation of the [HistoryStorage](../../mozilla.components.concept.storage/-history-storage/index.md) which is backed by a Rust Places lib via [PlacesApi](#). |

### Functions

| Name | Summary |
|---|---|
| [deleteEverything](delete-everything.md) | `open suspend fun deleteEverything(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sync behaviour: will not remove any history from remote devices, but it will prevent deleted history from returning. |
| [deleteVisit](delete-visit.md) | `open suspend fun deleteVisit(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, timestamp: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sync behaviour: will remove history from remote devices if this was the only visit for [url](delete-visit.md#mozilla.components.browser.storage.sync.PlacesHistoryStorage$deleteVisit(kotlin.String, kotlin.Long)/url). Otherwise, remote devices are not affected. |
| [deleteVisitsBetween](delete-visits-between.md) | `open suspend fun deleteVisitsBetween(startTime: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, endTime: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sync behaviour: may remove history from remote devices, if the removed visits were the only ones for a URL. |
| [deleteVisitsFor](delete-visits-for.md) | `open suspend fun deleteVisitsFor(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sync behaviour: will remove history from remote devices. |
| [deleteVisitsSince](delete-visits-since.md) | `open suspend fun deleteVisitsSince(since: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sync behaviour: may remove history from remote devices, if the removed visits were the only ones for a URL. |
| [getAutocompleteSuggestion](get-autocomplete-suggestion.md) | `open fun getAutocompleteSuggestion(query: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`HistoryAutocompleteResult`](../../mozilla.components.concept.storage/-history-autocomplete-result/index.md)`?`<br>Retrieves domain suggestions which best match the [query](../../mozilla.components.concept.storage/-history-storage/get-autocomplete-suggestion.md#mozilla.components.concept.storage.HistoryStorage$getAutocompleteSuggestion(kotlin.String)/query). |
| [getDetailedVisits](get-detailed-visits.md) | `open suspend fun getDetailedVisits(start: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, end: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`VisitInfo`](../../mozilla.components.concept.storage/-visit-info/index.md)`>`<br>Retrieves detailed information about all visits that occurred in the given time range. |
| [getSuggestions](get-suggestions.md) | `open fun getSuggestions(query: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, limit: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`SearchResult`](../../mozilla.components.concept.storage/-search-result/index.md)`>`<br>Retrieves suggestions matching the [query](../../mozilla.components.concept.storage/-history-storage/get-suggestions.md#mozilla.components.concept.storage.HistoryStorage$getSuggestions(kotlin.String, kotlin.Int)/query). |
| [getVisited](get-visited.md) | `open suspend fun getVisited(uris: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>`<br>Maps a list of page URIs to a list of booleans indicating if each URI was visited.`open suspend fun getVisited(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>Retrieves a list of all visited pages. |
| [prune](prune.md) | `open suspend fun prune(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Should only be called in response to severe disk storage pressure. May delete all of the data, or some subset of it. Sync behaviour: will not remove history from remote clients. |
| [recordObservation](record-observation.md) | `open suspend fun recordObservation(uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, observation: `[`PageObservation`](../../mozilla.components.concept.storage/-page-observation/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Records an observation about a page. |
| [recordVisit](record-visit.md) | `open suspend fun recordVisit(uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, visitType: `[`VisitType`](../../mozilla.components.concept.storage/-visit-type/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Records a visit to a page. |

### Inherited Functions

| Name | Summary |
|---|---|
| [cleanup](../-places-storage/cleanup.md) | `open fun cleanup(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Cleans up background work and database connections |
| [runMaintenance](../-places-storage/run-maintenance.md) | `open suspend fun runMaintenance(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Internal database maintenance tasks. Ideally this should be called once a day. |
| [sync](../-places-storage/sync.md) | `open suspend fun sync(authInfo: `[`AuthInfo`](../../mozilla.components.concept.sync/-auth-info/index.md)`): `[`SyncStatus`](../../mozilla.components.concept.sync/-sync-status/index.md)<br>Runs sync() method on the places Connection |
