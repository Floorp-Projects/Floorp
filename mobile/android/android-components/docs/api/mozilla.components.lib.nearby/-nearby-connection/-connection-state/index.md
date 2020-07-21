[android-components](../../../index.md) / [mozilla.components.lib.nearby](../../index.md) / [NearbyConnection](../index.md) / [ConnectionState](./index.md)

# ConnectionState

`sealed class ConnectionState` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/nearby/src/main/java/mozilla/components/lib/nearby/NearbyConnection.kt#L99)

The state of the connection. Changes in state are communicated to the client through
[NearbyConnectionObserver.onStateUpdated](../../-nearby-connection-observer/on-state-updated.md).

### Types

| Name | Summary |
|---|---|
| [Advertising](-advertising.md) | `object Advertising : `[`ConnectionState`](./index.md)<br>This device is advertising its presence. If it is discovered by another device, the next state will be [Authenticating](-authenticating/index.md). |
| [Authenticating](-authenticating/index.md) | `class Authenticating : `[`ConnectionState`](./index.md)<br>This device is in the process of authenticating with a neighboring device. If both devices accept the token, the next state will be [Connecting](-connecting/index.md). |
| [Connecting](-connecting/index.md) | `class Connecting : `[`ConnectionState`](./index.md)<br>The connection has been successfully authenticated. Unless an error occurs, the next state will be [ReadyToSend](-ready-to-send/index.md). |
| [Discovering](-discovering.md) | `object Discovering : `[`ConnectionState`](./index.md)<br>This device is trying to discover devices that are advertising. If it discovers one, the next state with be [Initiating](-initiating/index.md). |
| [Failure](-failure/index.md) | `class Failure : `[`ConnectionState`](./index.md)<br>A failure has occurred. |
| [Initiating](-initiating/index.md) | `class Initiating : `[`ConnectionState`](./index.md)<br>This device has discovered a neighboring device and is initiating a connection. If all goes well, the next state will be [Authenticating](-authenticating/index.md). |
| [Isolated](-isolated.md) | `object Isolated : `[`ConnectionState`](./index.md)<br>There is no connection to another device and no attempt to connect. |
| [ReadyToSend](-ready-to-send/index.md) | `class ReadyToSend : `[`ConnectionState`](./index.md)<br>A connection has been made to a neighbor and this device may send a message. This state is followed by [Sending](-sending/index.md) or [Failure](-failure/index.md). |
| [Sending](-sending/index.md) | `class Sending : `[`ConnectionState`](./index.md)<br>A message is being sent from this device. This state is followed by [ReadyToSend](-ready-to-send/index.md) or [Failure](-failure/index.md). |

### Properties

| Name | Summary |
|---|---|
| [name](name.md) | `val name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The name of the state, which is the same as the name of the concrete class (e.g., "Isolated" for [Isolated](-isolated.md)). |

### Inheritors

| Name | Summary |
|---|---|
| [Advertising](-advertising.md) | `object Advertising : `[`ConnectionState`](./index.md)<br>This device is advertising its presence. If it is discovered by another device, the next state will be [Authenticating](-authenticating/index.md). |
| [Authenticating](-authenticating/index.md) | `class Authenticating : `[`ConnectionState`](./index.md)<br>This device is in the process of authenticating with a neighboring device. If both devices accept the token, the next state will be [Connecting](-connecting/index.md). |
| [Connecting](-connecting/index.md) | `class Connecting : `[`ConnectionState`](./index.md)<br>The connection has been successfully authenticated. Unless an error occurs, the next state will be [ReadyToSend](-ready-to-send/index.md). |
| [Discovering](-discovering.md) | `object Discovering : `[`ConnectionState`](./index.md)<br>This device is trying to discover devices that are advertising. If it discovers one, the next state with be [Initiating](-initiating/index.md). |
| [Failure](-failure/index.md) | `class Failure : `[`ConnectionState`](./index.md)<br>A failure has occurred. |
| [Initiating](-initiating/index.md) | `class Initiating : `[`ConnectionState`](./index.md)<br>This device has discovered a neighboring device and is initiating a connection. If all goes well, the next state will be [Authenticating](-authenticating/index.md). |
| [Isolated](-isolated.md) | `object Isolated : `[`ConnectionState`](./index.md)<br>There is no connection to another device and no attempt to connect. |
| [ReadyToSend](-ready-to-send/index.md) | `class ReadyToSend : `[`ConnectionState`](./index.md)<br>A connection has been made to a neighbor and this device may send a message. This state is followed by [Sending](-sending/index.md) or [Failure](-failure/index.md). |
| [Sending](-sending/index.md) | `class Sending : `[`ConnectionState`](./index.md)<br>A message is being sent from this device. This state is followed by [ReadyToSend](-ready-to-send/index.md) or [Failure](-failure/index.md). |
