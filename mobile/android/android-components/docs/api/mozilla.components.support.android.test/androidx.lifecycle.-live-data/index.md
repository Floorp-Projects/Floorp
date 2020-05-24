[android-components](../../index.md) / [mozilla.components.support.android.test](../index.md) / [androidx.lifecycle.LiveData](./index.md)

### Extensions for androidx.lifecycle.LiveData

| Name | Summary |
|---|---|
| [awaitValue](await-value.md) | `fun <T> LiveData<`[`T`](await-value.md#T)`>.awaitValue(timeout: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = 1, unit: `[`TimeUnit`](http://docs.oracle.com/javase/7/docs/api/java/util/concurrent/TimeUnit.html)` = TimeUnit.SECONDS): `[`T`](await-value.md#T)`?`<br>Subscribes to the [LiveData](#) object and blocks until a value was observed. Returns the value or throws [InterruptedException](http://docs.oracle.com/javase/7/docs/api/java/lang/InterruptedException.html) if no value was observed in the given [timeout](await-value.md#mozilla.components.support.android.test$awaitValue(androidx.lifecycle.LiveData((mozilla.components.support.android.test.awaitValue.T)), kotlin.Long, java.util.concurrent.TimeUnit)/timeout) (using [unit](await-value.md#mozilla.components.support.android.test$awaitValue(androidx.lifecycle.LiveData((mozilla.components.support.android.test.awaitValue.T)), kotlin.Long, java.util.concurrent.TimeUnit)/unit)). |
