[android-components](../../index.md) / [mozilla.components.lib.nearby](../index.md) / [NearbyConnectionObserver](index.md) / [onMessageReceived](./on-message-received.md)

# onMessageReceived

`abstract fun onMessageReceived(neighborId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, neighborName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, message: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/nearby/src/main/java/mozilla/components/lib/nearby/NearbyConnection.kt#L459)

Called when a message is received from a neighboring device.

### Parameters

`neighborId` - the ID of the neighboring device

`neighborName` - the name of the neighboring device

`message` - the message