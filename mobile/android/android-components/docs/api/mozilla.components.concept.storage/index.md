[android-components](../index.md) / [mozilla.components.concept.storage](./index.md)

## Package mozilla.components.concept.storage

### Types

| Name | Summary |
|---|---|
| [HistoryAutocompleteResult](-history-autocomplete-result/index.md) | `data class HistoryAutocompleteResult`<br>Describes an autocompletion result against history storage. |
| [HistoryStorage](-history-storage/index.md) | `interface HistoryStorage`<br>An interface which defines read/write methods for history data. |
| [PageObservation](-page-observation/index.md) | `data class PageObservation` |
| [SearchResult](-search-result/index.md) | `data class SearchResult`<br>Encapsulates a set of properties which define a result of querying history storage. |
| [SyncError](-sync-error/index.md) | `data class SyncError : `[`SyncStatus`](-sync-status.md) |
| [SyncOk](-sync-ok.md) | `object SyncOk : `[`SyncStatus`](-sync-status.md) |
| [SyncStatus](-sync-status.md) | `sealed class SyncStatus` |
| [SyncableStore](-syncable-store/index.md) | `interface SyncableStore<AuthInfo>`<br>Describes a "sync" entry point for store which operates over [AuthInfo](-syncable-store/index.md#AuthInfo). |
| [VisitType](-visit-type/index.md) | `enum class VisitType`<br>Visit type constants as defined by Desktop Firefox. |
