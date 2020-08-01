[android-components](../../index.md) / [mozilla.components.support.base.coroutines](../index.md) / [Dispatchers](./index.md)

# Dispatchers

`object Dispatchers` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/coroutines/Dispatchers.kt#L17)

Shared [CoroutineDispatcher](#)s used by Android Components and app code - in addition to dispatchers
provided by `kotlinx-coroutines-android`.

### Properties

| Name | Summary |
|---|---|
| [Cached](-cached.md) | `val Cached: ExecutorCoroutineDispatcher`<br>[CoroutineDispatcher](#) for short-lived asynchronous tasks. This dispatcher is using a thread pool that creates new threads as needed, but will reuse previously constructed threads when they are available. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
