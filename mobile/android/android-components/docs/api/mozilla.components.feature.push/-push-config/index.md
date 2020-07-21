[android-components](../../index.md) / [mozilla.components.feature.push](../index.md) / [PushConfig](./index.md)

# PushConfig

`data class PushConfig` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/push/src/main/java/mozilla/components/feature/push/AutoPushFeature.kt#L410)

Configuration object for initializing the Push Manager with an AutoPush server.

### Parameters

`senderId` - The project identifier set by the server. Contact your server ops team to know what value to set.

`serverHost` - The sync server address.

`protocol` - The socket protocol to use when communicating with the server.

`serviceType` - The push services that the AutoPush server supports.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `PushConfig(senderId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, serverHost: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "updates.push.services.mozilla.com", protocol: `[`Protocol`](../-protocol/index.md)` = Protocol.HTTPS, serviceType: `[`ServiceType`](../-service-type/index.md)` = ServiceType.FCM)`<br>Configuration object for initializing the Push Manager with an AutoPush server. |

### Properties

| Name | Summary |
|---|---|
| [protocol](protocol.md) | `val protocol: `[`Protocol`](../-protocol/index.md)<br>The socket protocol to use when communicating with the server. |
| [senderId](sender-id.md) | `val senderId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The project identifier set by the server. Contact your server ops team to know what value to set. |
| [serverHost](server-host.md) | `val serverHost: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The sync server address. |
| [serviceType](service-type.md) | `val serviceType: `[`ServiceType`](../-service-type/index.md)<br>The push services that the AutoPush server supports. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
