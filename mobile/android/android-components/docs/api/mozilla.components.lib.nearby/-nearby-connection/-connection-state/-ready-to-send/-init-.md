[android-components](../../../../index.md) / [mozilla.components.lib.nearby](../../../index.md) / [NearbyConnection](../../index.md) / [ConnectionState](../index.md) / [ReadyToSend](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`ReadyToSend(neighborId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, neighborName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?)`

A connection has been made to a neighbor and this device may send a message.
This state is followed by [Sending](../-sending/index.md) or [Failure](../-failure/index.md).

### Parameters

`neighborId` - the neighbor's ID, which is not meant for human readability

`neighborName` - the neighbor's human-readable name