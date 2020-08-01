[android-components](../../index.md) / [mozilla.components.feature.push](../index.md) / [PushConfig](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`PushConfig(senderId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, serverHost: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "updates.push.services.mozilla.com", protocol: `[`Protocol`](../-protocol/index.md)` = Protocol.HTTPS, serviceType: `[`ServiceType`](../-service-type/index.md)` = ServiceType.FCM)`

Configuration object for initializing the Push Manager with an AutoPush server.

### Parameters

`senderId` - The project identifier set by the server. Contact your server ops team to know what value to set.

`serverHost` - The sync server address.

`protocol` - The socket protocol to use when communicating with the server.

`serviceType` - The push services that the AutoPush server supports.