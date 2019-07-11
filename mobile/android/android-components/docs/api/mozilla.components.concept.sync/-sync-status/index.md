[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [SyncStatus](./index.md)

# SyncStatus

`sealed class SyncStatus` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Sync.kt#L10)

Results of running a sync via [SyncableStore.sync](../-syncable-store/sync.md).

### Types

| Name | Summary |
|---|---|
| [Error](-error/index.md) | `data class Error : `[`SyncStatus`](./index.md)<br>Sync completed with an error. |
| [Ok](-ok.md) | `object Ok : `[`SyncStatus`](./index.md)<br>Sync succeeded successfully. |

### Inheritors

| Name | Summary |
|---|---|
| [Error](-error/index.md) | `data class Error : `[`SyncStatus`](./index.md)<br>Sync completed with an error. |
| [Ok](-ok.md) | `object Ok : `[`SyncStatus`](./index.md)<br>Sync succeeded successfully. |
