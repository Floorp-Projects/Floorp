[android-components](../../../../index.md) / [mozilla.components.lib.nearby](../../../index.md) / [NearbyConnection](../../index.md) / [ConnectionState](../index.md) / [Initiating](./index.md)

# Initiating

`class Initiating : `[`ConnectionState`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/nearby/src/main/java/mozilla/components/lib/nearby/NearbyConnection.kt#L127)

This device has discovered a neighboring device and is initiating a connection.
If all goes well, the next state will be [Authenticating](../-authenticating/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Initiating(neighborId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, neighborName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>This device has discovered a neighboring device and is initiating a connection. If all goes well, the next state will be [Authenticating](../-authenticating/index.md). |

### Properties

| Name | Summary |
|---|---|
| [neighborId](neighbor-id.md) | `val neighborId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The ID of the neighbor, which is not meant for human readability. |
| [neighborName](neighbor-name.md) | `val neighborName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The human readable name of the neighbor. |

### Inherited Properties

| Name | Summary |
|---|---|
| [name](../name.md) | `val name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The name of the state, which is the same as the name of the concrete class (e.g., "Isolated" for [Isolated](../-isolated.md)). |
