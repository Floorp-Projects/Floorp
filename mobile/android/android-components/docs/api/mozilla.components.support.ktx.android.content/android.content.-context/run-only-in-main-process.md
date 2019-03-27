[android-components](../../index.md) / [mozilla.components.support.ktx.android.content](../index.md) / [android.content.Context](index.md) / [runOnlyInMainProcess](./run-only-in-main-process.md)

# runOnlyInMainProcess

`inline fun `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.runOnlyInMainProcess(block: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/android/content/Context.kt#L117)

Takes a function runs it only it if we are running in the main process, otherwise the function will not be executed.

### Parameters

`block` - function to be executed in the main process.