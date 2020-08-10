[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesStorage](index.md) / [syncAndHandleExceptions](./sync-and-handle-exceptions.md)

# syncAndHandleExceptions

`protected inline fun syncAndHandleExceptions(syncBlock: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`SyncStatus`](../../mozilla.components.concept.sync/-sync-status/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesStorage.kt#L82)

Runs a [syncBlock](sync-and-handle-exceptions.md#mozilla.components.browser.storage.sync.PlacesStorage$syncAndHandleExceptions(kotlin.Function0((kotlin.Unit)))/syncBlock), re-throwing any panics that may be encountered.

**Return**
[SyncStatus.Ok](../../mozilla.components.concept.sync/-sync-status/-ok.md) on success, or [SyncStatus.Error](../../mozilla.components.concept.sync/-sync-status/-error/index.md) on non-panic [PlacesException](#).

