[android-components](../../index.md) / [mozilla.components.support.utils](../index.md) / [ThreadUtils](./index.md)

# ThreadUtils

`object ThreadUtils` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/utils/src/main/java/mozilla/components/support/utils/ThreadUtils.kt#L12)

### Functions

| Name | Summary |
|---|---|
| [assertOnUiThread](assert-on-ui-thread.md) | `fun assertOnUiThread(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [postToBackgroundThread](post-to-background-thread.md) | `fun postToBackgroundThread(runnable: `[`Runnable`](https://developer.android.com/reference/java/lang/Runnable.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>`fun postToBackgroundThread(runnable: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [postToMainThread](post-to-main-thread.md) | `fun postToMainThread(runnable: `[`Runnable`](https://developer.android.com/reference/java/lang/Runnable.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [postToMainThreadDelayed](post-to-main-thread-delayed.md) | `fun postToMainThreadDelayed(runnable: `[`Runnable`](https://developer.android.com/reference/java/lang/Runnable.html)`, delayMillis: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setHandlerForTest](set-handler-for-test.md) | `fun setHandlerForTest(handler: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
