[android-components](../../index.md) / [mozilla.components.service.pocket.net](../index.md) / [PocketResponse](./index.md)

# PocketResponse

`sealed class PocketResponse<T>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/pocket/src/main/java/mozilla/components/service/pocket/net/PocketResponse.kt#L10)

A response from the Pocket API: the subclasses determine the type of the result and contain usable data.

### Types

| Name | Summary |
|---|---|
| [Failure](-failure.md) | `class Failure<T> : `[`PocketResponse`](./index.md)`<`[`T`](-failure.md#T)`>`<br>A failure response from the Pocket API. |
| [Success](-success/index.md) | `data class Success<T> : `[`PocketResponse`](./index.md)`<`[`T`](-success/index.md#T)`>`<br>A successful response from the Pocket API. |

### Inheritors

| Name | Summary |
|---|---|
| [Failure](-failure.md) | `class Failure<T> : `[`PocketResponse`](./index.md)`<`[`T`](-failure.md#T)`>`<br>A failure response from the Pocket API. |
| [Success](-success/index.md) | `data class Success<T> : `[`PocketResponse`](./index.md)`<`[`T`](-success/index.md#T)`>`<br>A successful response from the Pocket API. |
