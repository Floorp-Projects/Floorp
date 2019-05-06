[android-components](../../index.md) / [mozilla.components.support.android.test](../index.md) / [androidx.lifecycle.LiveData](index.md) / [awaitValue](./await-value.md)

# awaitValue

`fun <T> LiveData<`[`T`](await-value.md#T)`>.awaitValue(timeout: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = 1, unit: `[`TimeUnit`](https://developer.android.com/reference/java/util/concurrent/TimeUnit.html)` = TimeUnit.SECONDS): `[`T`](await-value.md#T)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/android-test/src/main/java/mozilla/components/support/android/test/LiveData.kt#L20)

Subscribes to the [LiveData](#) object and blocks until a value was observed. Returns the value or throws
[InterruptedException](https://developer.android.com/reference/java/lang/InterruptedException.html) if no value was observed in the given [timeout](await-value.md#mozilla.components.support.android.test$awaitValue(androidx.lifecycle.LiveData((mozilla.components.support.android.test.awaitValue.T)), kotlin.Long, java.util.concurrent.TimeUnit)/timeout) (using [unit](await-value.md#mozilla.components.support.android.test$awaitValue(androidx.lifecycle.LiveData((mozilla.components.support.android.test.awaitValue.T)), kotlin.Long, java.util.concurrent.TimeUnit)/unit)).

This is helpful for tests using [LiveData](#) objects that won't contain any data unless observed (e.g. LiveData objects
returned by Room).

