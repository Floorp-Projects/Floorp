[android-components](../../../index.md) / [mozilla.components.concept.engine.request](../../index.md) / [RequestInterceptor](../index.md) / [InterceptionResponse](./index.md)

# InterceptionResponse

`sealed class InterceptionResponse` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/request/RequestInterceptor.kt#L19)

An alternative response for an intercepted request.

### Types

| Name | Summary |
|---|---|
| [AppIntent](-app-intent/index.md) | `data class AppIntent : `[`InterceptionResponse`](./index.md) |
| [Content](-content/index.md) | `data class Content : `[`InterceptionResponse`](./index.md) |
| [Deny](-deny.md) | `object Deny : `[`InterceptionResponse`](./index.md)<br>Deny request without further action. |
| [Url](-url/index.md) | `data class Url : `[`InterceptionResponse`](./index.md) |

### Inheritors

| Name | Summary |
|---|---|
| [AppIntent](-app-intent/index.md) | `data class AppIntent : `[`InterceptionResponse`](./index.md) |
| [Content](-content/index.md) | `data class Content : `[`InterceptionResponse`](./index.md) |
| [Deny](-deny.md) | `object Deny : `[`InterceptionResponse`](./index.md)<br>Deny request without further action. |
| [Url](-url/index.md) | `data class Url : `[`InterceptionResponse`](./index.md) |
