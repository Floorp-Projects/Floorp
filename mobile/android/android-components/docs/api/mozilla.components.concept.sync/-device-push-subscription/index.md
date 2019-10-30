[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [DevicePushSubscription](./index.md)

# DevicePushSubscription

`data class DevicePushSubscription` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Devices.kt#L125)

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
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
