[android-components](../index.md) / [mozilla.components.support.ktx.kotlinx.coroutines](index.md) / [throttleLatest](./throttle-latest.md)

# throttleLatest

`fun <T> throttleLatest(skipTimeInMs: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = 300L, coroutineScope: CoroutineScope, block: (`[`T`](throttle-latest.md#T)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): (`[`T`](throttle-latest.md#T)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/kotlinx/coroutines/Utils.kt#L24)

Returns a function that limits the executions of the [block](throttle-latest.md#mozilla.components.support.ktx.kotlinx.coroutines$throttleLatest(kotlin.Long, kotlinx.coroutines.CoroutineScope, kotlin.Function1((mozilla.components.support.ktx.kotlinx.coroutines.throttleLatest.T, kotlin.Unit)))/block) function, until the [skipTimeInMs](throttle-latest.md#mozilla.components.support.ktx.kotlinx.coroutines$throttleLatest(kotlin.Long, kotlinx.coroutines.CoroutineScope, kotlin.Function1((mozilla.components.support.ktx.kotlinx.coroutines.throttleLatest.T, kotlin.Unit)))/skipTimeInMs)
passes, then the latest value passed to [block](throttle-latest.md#mozilla.components.support.ktx.kotlinx.coroutines$throttleLatest(kotlin.Long, kotlinx.coroutines.CoroutineScope, kotlin.Function1((mozilla.components.support.ktx.kotlinx.coroutines.throttleLatest.T, kotlin.Unit)))/block) will be used. Any calls before [skipTimeInMs](throttle-latest.md#mozilla.components.support.ktx.kotlinx.coroutines$throttleLatest(kotlin.Long, kotlinx.coroutines.CoroutineScope, kotlin.Function1((mozilla.components.support.ktx.kotlinx.coroutines.throttleLatest.T, kotlin.Unit)))/skipTimeInMs)
passes will be ignored. All calls to the returned function must happen on the same thread.

Credit to Terenfear https://gist.github.com/Terenfear/a84863be501d3399889455f391eeefe5

### Parameters

`skipTimeInMs` - the time to wait until the next call to [block](throttle-latest.md#mozilla.components.support.ktx.kotlinx.coroutines$throttleLatest(kotlin.Long, kotlinx.coroutines.CoroutineScope, kotlin.Function1((mozilla.components.support.ktx.kotlinx.coroutines.throttleLatest.T, kotlin.Unit)))/block) be processed.

`coroutineScope` - the coroutine scope where [block](throttle-latest.md#mozilla.components.support.ktx.kotlinx.coroutines$throttleLatest(kotlin.Long, kotlinx.coroutines.CoroutineScope, kotlin.Function1((mozilla.components.support.ktx.kotlinx.coroutines.throttleLatest.T, kotlin.Unit)))/block) will executed.

`block` - function to be execute.