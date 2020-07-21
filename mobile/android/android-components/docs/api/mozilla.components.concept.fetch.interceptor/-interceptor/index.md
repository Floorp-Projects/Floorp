[android-components](../../index.md) / [mozilla.components.concept.fetch.interceptor](../index.md) / [Interceptor](./index.md)

# Interceptor

`interface Interceptor` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/fetch/src/main/java/mozilla/components/concept/fetch/interceptor/Interceptor.kt#L16)

An [Interceptor](./index.md) for a [Client](../../mozilla.components.concept.fetch/-client/index.md) implementation.

Interceptors can monitor, modify, retry, redirect or record requests as well as responses going through a [Client](../../mozilla.components.concept.fetch/-client/index.md).

### Types

| Name | Summary |
|---|---|
| [Chain](-chain/index.md) | `interface Chain`<br>The request interceptor chain. |

### Functions

| Name | Summary |
|---|---|
| [intercept](intercept.md) | `abstract fun intercept(chain: `[`Chain`](-chain/index.md)`): `[`Response`](../../mozilla.components.concept.fetch/-response/index.md)<br>Allows an [Interceptor](./index.md) to intercept a request and modify request or response. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
