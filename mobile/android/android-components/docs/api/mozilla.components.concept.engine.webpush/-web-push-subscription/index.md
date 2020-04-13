[android-components](../../index.md) / [mozilla.components.concept.engine.webpush](../index.md) / [WebPushSubscription](./index.md)

# WebPushSubscription

`data class WebPushSubscription` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webpush/WebPush.kt#L43)

A data class representation of the [PushSubscription](https://developer.mozilla.org/en-US/docs/Web/API/PushSubscription) web specification.

### Parameters

`scope` - The subscription identifier which usually represents the website's URI.

`endpoint` - The Web Push endpoint for this subscription.
This is the URL of a web service which implements the Web Push protocol.

`appServerKey` - A public key a server will use to send messages to client apps via a push server.

`publicKey` - The public key generated, to be provided to the app server for message encryption.

`authSecret` - A secret key generated, to be provided to the app server for use in encrypting
and authenticating messages sent to the endpoint.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WebPushSubscription(scope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, endpoint: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, appServerKey: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`?, publicKey: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`, authSecret: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`)`<br>A data class representation of the [PushSubscription](https://developer.mozilla.org/en-US/docs/Web/API/PushSubscription) web specification. |

### Properties

| Name | Summary |
|---|---|
| [appServerKey](app-server-key.md) | `val appServerKey: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`?`<br>A public key a server will use to send messages to client apps via a push server. |
| [authSecret](auth-secret.md) | `val authSecret: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)<br>A secret key generated, to be provided to the app server for use in encrypting and authenticating messages sent to the endpoint. |
| [endpoint](endpoint.md) | `val endpoint: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The Web Push endpoint for this subscription. This is the URL of a web service which implements the Web Push protocol. |
| [publicKey](public-key.md) | `val publicKey: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)<br>The public key generated, to be provided to the app server for message encryption. |
| [scope](scope.md) | `val scope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The subscription identifier which usually represents the website's URI. |

### Functions

| Name | Summary |
|---|---|
| [equals](equals.md) | `fun equals(other: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [hashCode](hash-code.md) | `fun hashCode(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
