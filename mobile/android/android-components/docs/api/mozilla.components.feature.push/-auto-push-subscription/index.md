[android-components](../../index.md) / [mozilla.components.feature.push](../index.md) / [AutoPushSubscription](./index.md)

# AutoPushSubscription

`data class AutoPushSubscription` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/push/src/main/java/mozilla/components/feature/push/AutoPushFeature.kt#L344)

The subscription information from Autopush that can be used to send push messages to other devices.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AutoPushSubscription(type: `[`PushType`](../-push-type/index.md)`, endpoint: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, publicKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, authKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>The subscription information from Autopush that can be used to send push messages to other devices. |

### Properties

| Name | Summary |
|---|---|
| [authKey](auth-key.md) | `val authKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [endpoint](endpoint.md) | `val endpoint: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [publicKey](public-key.md) | `val publicKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [type](type.md) | `val type: `[`PushType`](../-push-type/index.md) |
