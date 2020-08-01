[android-components](../../../../index.md) / [mozilla.components.lib.nearby](../../../index.md) / [NearbyConnection](../../index.md) / [ConnectionState](../index.md) / [Authenticating](./index.md)

# Authenticating

`class Authenticating : `[`ConnectionState`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/nearby/src/main/java/mozilla/components/lib/nearby/NearbyConnection.kt#L143)

This device is in the process of authenticating with a neighboring device. If both
devices accept the token, the next state will be [Connecting](../-connecting/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Authenticating(nearbyConnection: `[`NearbyConnection`](../../index.md)`, fallbackState: `[`ConnectionState`](../index.md)`, neighborId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, neighborName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, token: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>This device is in the process of authenticating with a neighboring device. If both devices accept the token, the next state will be [Connecting](../-connecting/index.md). |

### Properties

| Name | Summary |
|---|---|
| [neighborId](neighbor-id.md) | `val neighborId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The ID of the neighbor, which is not meant for human readability. |
| [neighborName](neighbor-name.md) | `val neighborName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The human readable name of the neighbor. |
| [token](token.md) | `val token: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>A short unique token of printable characters shared by both sides of the pending connection. |

### Inherited Properties

| Name | Summary |
|---|---|
| [name](../name.md) | `val name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The name of the state, which is the same as the name of the concrete class (e.g., "Isolated" for [Isolated](../-isolated.md)). |

### Functions

| Name | Summary |
|---|---|
| [accept](accept.md) | `fun accept(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Accepts the connection to the neighbor. |
| [reject](reject.md) | `fun reject(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Rejects the connection to the neighbor. |
