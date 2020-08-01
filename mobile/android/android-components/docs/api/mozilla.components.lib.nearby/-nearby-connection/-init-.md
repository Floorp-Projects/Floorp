[android-components](../../index.md) / [mozilla.components.lib.nearby](../index.md) / [NearbyConnection](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`NearbyConnection(context: <ERROR CLASS>, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = Build.MODEL)`

Another constructor

`NearbyConnection(connectionsClient: ConnectionsClient, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = Build.MODEL, delegate: `[`ObserverRegistry`](../../mozilla.components.support.base.observer/-observer-registry/index.md)`<`[`NearbyConnectionObserver`](../-nearby-connection-observer/index.md)`> = ObserverRegistry())`

Constructs a new connection, which will call [NearbyConnectionObserver.onStateUpdated](../-nearby-connection-observer/on-state-updated.md)
    with an argument of type [ConnectionState.Isolated](-connection-state/-isolated.md). No further action will be taken unless
    other methods are called by the client.

### Parameters

`connectionsClient` - the underlying client

`name` - a human-readable name for this device

**Constructor**
Constructs a new connection, which will call [NearbyConnectionObserver.onStateUpdated](../-nearby-connection-observer/on-state-updated.md)
    with an argument of type [ConnectionState.Isolated](-connection-state/-isolated.md). No further action will be taken unless
    other methods are called by the client.

