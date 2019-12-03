[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [SyncStatus](./index.md)

# SyncStatus

`sealed class SyncStatus` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Sync.kt#L10)

Results of running a sync via [SyncableStore.sync](#).

### Types

| Name | Summary |
|---|---|
| [Error](-error/index.md) | `data class Error : `[`SyncStatus`](./index.md)<br>Sync completed with an error. |
| [Ok](-ok.md) | `object Ok : `[`SyncStatus`](./index.md)<br>Sync succeeded successfully. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [Error](-error/index.md) | `data class Error : `[`SyncStatus`](./index.md)<br>Sync completed with an error. |
| [Ok](-ok.md) | `object Ok : `[`SyncStatus`](./index.md)<br>Sync succeeded successfully. |
