[android-components](../../../../index.md) / [mozilla.components.lib.nearby](../../../index.md) / [NearbyConnection](../../index.md) / [ConnectionState](../index.md) / [Sending](./index.md)

# Sending

`class Sending : `[`ConnectionState`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/nearby/src/main/java/mozilla/components/lib/nearby/NearbyConnection.kt#L215)

A message is being sent from this device. This state is followed by [ReadyToSend](../-ready-to-send/index.md) or
[Failure](../-failure/index.md).

### Parameters

`neighborId` - the neighbor's ID, which is not meant for human readability

`neighborName` - the neighbor's human-readable name

`payloadId` - the ID of the message that was sent, which will appear again
in [NearbyConnectionObserver.onMessageDelivered](../../../-nearby-connection-observer/on-message-delivered.md)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Sending(neighborId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, neighborName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, payloadId: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`)`<br>A message is being sent from this device. This state is followed by [ReadyToSend](../-ready-to-send/index.md) or [Failure](../-failure/index.md). |

### Properties

| Name | Summary |
|---|---|
| [neighborId](neighbor-id.md) | `val neighborId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the neighbor's ID, which is not meant for human readability |
| [neighborName](neighbor-name.md) | `val neighborName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>the neighbor's human-readable name |
| [payloadId](payload-id.md) | `val payloadId: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>the ID of the message that was sent, which will appear again in [NearbyConnectionObserver.onMessageDelivered](../../../-nearby-connection-observer/on-message-delivered.md) |

### Inherited Properties

| Name | Summary |
|---|---|
| [name](../name.md) | `val name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The name of the state, which is the same as the name of the concrete class (e.g., "Isolated" for [Isolated](../-isolated.md)). |
