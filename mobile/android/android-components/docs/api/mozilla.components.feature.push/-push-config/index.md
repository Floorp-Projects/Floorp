[android-components](../../index.md) / [mozilla.components.feature.push](../index.md) / [PushConfig](./index.md)

# PushConfig

`data class PushConfig` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/push/src/main/java/mozilla/components/feature/push/AutoPushFeature.kt#L354)

Configuration object for initializing the Push Manager.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `PushConfig(senderId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, serverHost: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "updates.push.services.mozilla.com", protocol: `[`Protocol`](../-protocol/index.md)` = Protocol.HTTPS, serviceType: `[`ServiceType`](../-service-type/index.md)` = ServiceType.FCM)`<br>Configuration object for initializing the Push Manager. |

### Properties

| Name | Summary |
|---|---|
| [protocol](protocol.md) | `val protocol: `[`Protocol`](../-protocol/index.md) |
| [senderId](sender-id.md) | `val senderId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [serverHost](server-host.md) | `val serverHost: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [serviceType](service-type.md) | `val serviceType: `[`ServiceType`](../-service-type/index.md) |
