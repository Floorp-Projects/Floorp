[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [SyncStatusObserver](index.md) / [onError](./on-error.md)

# onError

`abstract fun onError(error: `[`Exception`](https://developer.android.com/reference/java/lang/Exception.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Sync.kt#L95)

Gets called if sync encounters an error that's worthy of processing by status observers.

### Parameters

`error` - Optional relevant exception.