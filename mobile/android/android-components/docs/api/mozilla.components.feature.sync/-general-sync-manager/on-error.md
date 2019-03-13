[android-components](../../index.md) / [mozilla.components.feature.sync](../index.md) / [GeneralSyncManager](index.md) / [onError](./on-error.md)

# onError

`open fun onError(error: `[`Exception`](https://developer.android.com/reference/java/lang/Exception.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/sync/src/main/java/mozilla/components/feature/sync/BackgroundSyncManager.kt#L176)

Overrides [SyncStatusObserver.onError](../../mozilla.components.concept.sync/-sync-status-observer/on-error.md)

Gets called if sync encounters an error that's worthy of processing by status observers.

### Parameters

`error` - Optional relevant exception.