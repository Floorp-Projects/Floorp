[android-components](../../index.md) / [mozilla.components.support.utils](../index.md) / [ThreadUtils](./index.md)

# ThreadUtils

`object ThreadUtils` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/utils/src/main/java/mozilla/components/support/utils/ThreadUtils.kt#L12)

### Functions

| Name | Summary |
|---|---|
| [assertOnUiThread](assert-on-ui-thread.md) | `fun assertOnUiThread(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [postToBackgroundThread](post-to-background-thread.md) | `fun postToBackgroundThread(runnable: `[`Runnable`](http://docs.oracle.com/javase/7/docs/api/java/lang/Runnable.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>`fun postToBackgroundThread(runnable: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [postToMainThread](post-to-main-thread.md) | `fun postToMainThread(runnable: `[`Runnable`](http://docs.oracle.com/javase/7/docs/api/java/lang/Runnable.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [postToMainThreadDelayed](post-to-main-thread-delayed.md) | `fun postToMainThreadDelayed(runnable: `[`Runnable`](http://docs.oracle.com/javase/7/docs/api/java/lang/Runnable.html)`, delayMillis: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setHandlerForTest](set-handler-for-test.md) | `fun setHandlerForTest(handler: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
