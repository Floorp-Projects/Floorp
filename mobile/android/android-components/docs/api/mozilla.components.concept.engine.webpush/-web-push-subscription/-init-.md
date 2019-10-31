[android-components](../../index.md) / [mozilla.components.concept.engine.webpush](../index.md) / [WebPushSubscription](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`WebPushSubscription(scope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, endpoint: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, appServerKey: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`?, publicKey: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`, authSecret: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`)`

A data class representation of the [PushSubscription](https://developer.mozilla.org/en-US/docs/Web/API/PushSubscription) web specification.

### Parameters

`scope` - The subscription identifier which usually represents the website's URI.

`endpoint` - The Web Push endpoint for this subscription.
This is the URL of a web service which implements the Web Push protocol.

`appServerKey` - A public key a server will use to send messages to client apps via a push server.

`publicKey` - The public key generated, to be provided to the app server for message encryption.

`authSecret` - A secret key generated, to be provided to the app server for use in encrypting
and authenticating messages sent to the endpoint.