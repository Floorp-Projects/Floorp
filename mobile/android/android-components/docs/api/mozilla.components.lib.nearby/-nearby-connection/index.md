[android-components](../../index.md) / [mozilla.components.lib.nearby](../index.md) / [NearbyConnection](./index.md)

# NearbyConnection

`class NearbyConnection : `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`NearbyConnectionObserver`](../-nearby-connection-observer/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/nearby/src/main/java/mozilla/components/lib/nearby/NearbyConnection.kt#L74)

A class that can be run on two devices to allow them to connect. This supports sending a single
message at a time in each direction. It contains internal synchronization and may be accessed
from any thread.

### Types

| Name | Summary |
|---|---|
| [ConnectionState](-connection-state/index.md) | `sealed class ConnectionState`<br>The state of the connection. Changes in state are communicated to the client through [NearbyConnectionObserver.onStateUpdated](../-nearby-connection-observer/on-state-updated.md). |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `NearbyConnection(context: <ERROR CLASS>, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = Build.MODEL)`<br>Another constructor`NearbyConnection(connectionsClient: ConnectionsClient, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = Build.MODEL, delegate: `[`ObserverRegistry`](../../mozilla.components.support.base.observer/-observer-registry/index.md)`<`[`NearbyConnectionObserver`](../-nearby-connection-observer/index.md)`> = ObserverRegistry())`<br>Constructs a new connection, which will call [NearbyConnectionObserver.onStateUpdated](../-nearby-connection-observer/on-state-updated.md)     with an argument of type [ConnectionState.Isolated](-connection-state/-isolated.md). No further action will be taken unless     other methods are called by the client. |

### Properties

| Name | Summary |
|---|---|
| [connectionState](connection-state.md) | `var connectionState: `[`ConnectionState`](-connection-state/index.md) |

### Functions

| Name | Summary |
|---|---|
| [disconnect](disconnect.md) | `fun disconnect(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Breaks any connections to neighboring devices. This also stops advertising and discovering. The state will be updated to [ConnectionState.Isolated](-connection-state/-isolated.md). It is important to call this when the connection is no longer needed because of a [leak in the GATT client](http://bit.ly/33VP1gn). |
| [register](register.md) | `fun register(observer: `[`NearbyConnectionObserver`](../-nearby-connection-observer/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>`fun register(observer: `[`NearbyConnectionObserver`](../-nearby-connection-observer/index.md)`, view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>`fun register(observer: `[`NearbyConnectionObserver`](../-nearby-connection-observer/index.md)`, owner: LifecycleOwner, autoPause: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers an observer to get notified about changes. |
| [sendMessage](send-message.md) | `fun sendMessage(message: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`?`<br>Sends a message to a connected device. If the current state is not [ConnectionState.ReadyToSend](-connection-state/-ready-to-send/index.md), the message will not be sent. |
| [startAdvertising](start-advertising.md) | `fun startAdvertising(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts advertising this device. After calling this, the state will be updated to [ConnectionState.Advertising](-connection-state/-advertising.md) or [ConnectionState.Failure](-connection-state/-failure/index.md). If all goes well, eventually the state will be updated to [ConnectionState.Authenticating](-connection-state/-authenticating/index.md). A client should call either [startAdvertising](start-advertising.md) or [startDiscovering](start-discovering.md) to make a connection, not both. To initiate a connection, one device must call [startAdvertising](start-advertising.md) and the other [startDiscovering](start-discovering.md). |
| [startDiscovering](start-discovering.md) | `fun startDiscovering(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts trying to discover nearby advertising devices. After calling this, the state will be updated to [ConnectionState.Discovering](-connection-state/-discovering.md) or [ConnectionState.Failure](-connection-state/-failure/index.md). If all goes well, eventually the state will be updated to [ConnectionState.Authenticating](-connection-state/-authenticating/index.md). A client should call either [startAdvertising](start-advertising.md) or [startDiscovering](start-discovering.md) to make a connection, not both. To initiate a connection, one device must call [startAdvertising](start-advertising.md) and the other [startDiscovering](start-discovering.md). |

### Companion Object Properties

| Name | Summary |
|---|---|
| [PERMISSIONS](-p-e-r-m-i-s-s-i-o-n-s.md) | `val PERMISSIONS: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>The permissions needed by [NearbyConnection](./index.md). It is the client's responsibility to ensure that all are granted before constructing an instance of this class. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
