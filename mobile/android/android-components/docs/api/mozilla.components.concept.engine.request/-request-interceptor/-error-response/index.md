[android-components](../../../index.md) / [mozilla.components.concept.engine.request](../../index.md) / [RequestInterceptor](../index.md) / [ErrorResponse](./index.md)

# ErrorResponse

`sealed class ErrorResponse` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/request/RequestInterceptor.kt#L39)

An alternative response for an error request.

### Types

| Name | Summary |
|---|---|
| [Content](-content/index.md) | `data class Content : `[`ErrorResponse`](./index.md)<br>Used to load from data with base URL |
| [Uri](-uri/index.md) | `data class Uri : `[`ErrorResponse`](./index.md)<br>Used to load an encoded URI directly |

### Inheritors

| Name | Summary |
|---|---|
| [Content](-content/index.md) | `data class Content : `[`ErrorResponse`](./index.md)<br>Used to load from data with base URL |
| [Uri](-uri/index.md) | `data class Uri : `[`ErrorResponse`](./index.md)<br>Used to load an encoded URI directly |
