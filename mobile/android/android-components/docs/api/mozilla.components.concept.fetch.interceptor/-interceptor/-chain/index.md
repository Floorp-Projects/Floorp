[android-components](../../../index.md) / [mozilla.components.concept.fetch.interceptor](../../index.md) / [Interceptor](../index.md) / [Chain](./index.md)

# Chain

`interface Chain` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/fetch/src/main/java/mozilla/components/concept/fetch/interceptor/Interceptor.kt#L35)

The request interceptor chain.

### Properties

| Name | Summary |
|---|---|
| [request](request.md) | `abstract val request: `[`Request`](../../../mozilla.components.concept.fetch/-request/index.md)<br>The current request. May be modified by a previously executed interceptor. |

### Functions

| Name | Summary |
|---|---|
| [proceed](proceed.md) | `abstract fun proceed(request: `[`Request`](../../../mozilla.components.concept.fetch/-request/index.md)`): `[`Response`](../../../mozilla.components.concept.fetch/-response/index.md)<br>Proceed executing the interceptor chain and eventually perform the request. |
