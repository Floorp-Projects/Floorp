[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [Result](./index.md)

# Result

`sealed class Result<T>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/Result.kt#L10)

Class representing the result of a successful or failed migration action.

### Types

| Name | Summary |
|---|---|
| [Failure](-failure/index.md) | `class Failure<T> : `[`Result`](./index.md)`<`[`T`](-failure/index.md#T)`>`<br>Failed migration. |
| [Success](-success/index.md) | `class Success<T> : `[`Result`](./index.md)`<`[`T`](-success/index.md#T)`>`<br>Successful migration that produced data of type [T](-success/index.md#T). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [Failure](-failure/index.md) | `class Failure<T> : `[`Result`](./index.md)`<`[`T`](-failure/index.md#T)`>`<br>Failed migration. |
| [Success](-success/index.md) | `class Success<T> : `[`Result`](./index.md)`<`[`T`](-success/index.md#T)`>`<br>Successful migration that produced data of type [T](-success/index.md#T). |
