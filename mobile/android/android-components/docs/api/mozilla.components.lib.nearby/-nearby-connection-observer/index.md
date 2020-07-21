[android-components](../../index.md) / [mozilla.components.lib.nearby](../index.md) / [NearbyConnectionObserver](./index.md)

# NearbyConnectionObserver

`interface NearbyConnectionObserver` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/nearby/src/main/java/mozilla/components/lib/nearby/NearbyConnection.kt#L443)

Interface definition for observing changes in a [NearbyConnection](../-nearby-connection/index.md).

### Functions

| Name | Summary |
|---|---|
| [onMessageDelivered](on-message-delivered.md) | `abstract fun onMessageDelivered(payloadId: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called when a message has been successfully delivered to a neighboring device. |
| [onMessageReceived](on-message-received.md) | `abstract fun onMessageReceived(neighborId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, neighborName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, message: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called when a message is received from a neighboring device. |
| [onStateUpdated](on-state-updated.md) | `abstract fun onStateUpdated(connectionState: `[`ConnectionState`](../-nearby-connection/-connection-state/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called whenever the connection's state is set. In the absence of failures, the new state should differ from the prior state, but that is not guaranteed. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
