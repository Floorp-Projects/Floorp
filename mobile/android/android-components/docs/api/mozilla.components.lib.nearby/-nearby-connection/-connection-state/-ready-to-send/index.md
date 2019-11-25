[android-components](../../../../index.md) / [mozilla.components.lib.nearby](../../../index.md) / [NearbyConnection](../../index.md) / [ConnectionState](../index.md) / [ReadyToSend](./index.md)

# ReadyToSend

`class ReadyToSend : `[`ConnectionState`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/nearby/src/main/java/mozilla/components/lib/nearby/NearbyConnection.kt#L204)

A connection has been made to a neighbor and this device may send a message.
This state is followed by [Sending](../-sending/index.md) or [Failure](../-failure/index.md).

### Parameters

`neighborId` - the neighbor's ID, which is not meant for human readability

`neighborName` - the neighbor's human-readable name

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ReadyToSend(neighborId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, neighborName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?)`<br>A connection has been made to a neighbor and this device may send a message. This state is followed by [Sending](../-sending/index.md) or [Failure](../-failure/index.md). |

### Properties

| Name | Summary |
|---|---|
| [neighborId](neighbor-id.md) | `val neighborId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the neighbor's ID, which is not meant for human readability |
| [neighborName](neighbor-name.md) | `val neighborName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>the neighbor's human-readable name |

### Inherited Properties

| Name | Summary |
|---|---|
| [name](../name.md) | `val name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The name of the state, which is the same as the name of the concrete class (e.g., "Isolated" for [Isolated](../-isolated.md)). |
