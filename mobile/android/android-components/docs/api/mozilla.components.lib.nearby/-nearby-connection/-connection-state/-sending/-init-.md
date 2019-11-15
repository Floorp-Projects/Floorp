[android-components](../../../../index.md) / [mozilla.components.lib.nearby](../../../index.md) / [NearbyConnection](../../index.md) / [ConnectionState](../index.md) / [Sending](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`Sending(neighborId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, neighborName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, payloadId: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`)`

A message is being sent from this device. This state is followed by [ReadyToSend](../-ready-to-send/index.md) or
[Failure](../-failure/index.md).

### Parameters

`neighborId` - the neighbor's ID, which is not meant for human readability

`neighborName` - the neighbor's human-readable name

`payloadId` - the ID of the message that was sent, which will appear again
in [NearbyConnectionObserver.onMessageDelivered](../../../-nearby-connection-observer/on-message-delivered.md)