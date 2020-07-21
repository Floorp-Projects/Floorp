[android-components](../../index.md) / [mozilla.components.service.pocket.net](../index.md) / [PocketResponse](./index.md)

# PocketResponse

`sealed class PocketResponse<T>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/pocket/src/main/java/mozilla/components/service/pocket/net/PocketResponse.kt#L10)

A response from the Pocket API: the subclasses determine the type of the result and contain usable data.

### Types

| Name | Summary |
|---|---|
| [Failure](-failure.md) | `class Failure<T> : `[`PocketResponse`](./index.md)`<`[`T`](-failure.md#T)`>`<br>A failure response from the Pocket API. |
| [Success](-success/index.md) | `data class Success<T> : `[`PocketResponse`](./index.md)`<`[`T`](-success/index.md#T)`>`<br>A successful response from the Pocket API. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [Failure](-failure.md) | `class Failure<T> : `[`PocketResponse`](./index.md)`<`[`T`](-failure.md#T)`>`<br>A failure response from the Pocket API. |
| [Success](-success/index.md) | `data class Success<T> : `[`PocketResponse`](./index.md)`<`[`T`](-success/index.md#T)`>`<br>A successful response from the Pocket API. |
