[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [DevicePushSubscription](./index.md)

# DevicePushSubscription

`data class DevicePushSubscription` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Devices.kt#L136)

Describes an Autopush-compatible push channel subscription.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DevicePushSubscription(endpoint: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, publicKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, authKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>Describes an Autopush-compatible push channel subscription. |

### Properties

| Name | Summary |
|---|---|
| [authKey](auth-key.md) | `val authKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [endpoint](endpoint.md) | `val endpoint: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [publicKey](public-key.md) | `val publicKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [into](../../mozilla.components.service.fxa/into.md) | `fun `[`DevicePushSubscription`](./index.md)`.into(): PushSubscription` |
