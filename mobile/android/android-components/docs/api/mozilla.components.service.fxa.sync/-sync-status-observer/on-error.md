[android-components](../../index.md) / [mozilla.components.service.fxa.sync](../index.md) / [SyncStatusObserver](index.md) / [onError](./on-error.md)

# onError

`abstract fun onError(error: `[`Exception`](http://docs.oracle.com/javase/7/docs/api/java/lang/Exception.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/sync/SyncManager.kt#L68)

Gets called if sync encounters an error that's worthy of processing by status observers.

### Parameters

`error` - Optional relevant exception.