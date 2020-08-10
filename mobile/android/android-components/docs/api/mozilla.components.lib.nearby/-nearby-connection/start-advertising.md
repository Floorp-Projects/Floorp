[android-components](../../index.md) / [mozilla.components.lib.nearby](../index.md) / [NearbyConnection](index.md) / [startAdvertising](./start-advertising.md)

# startAdvertising

`fun startAdvertising(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/nearby/src/main/java/mozilla/components/lib/nearby/NearbyConnection.kt#L265)

Starts advertising this device. After calling this, the state will be updated to
[ConnectionState.Advertising](-connection-state/-advertising.md) or [ConnectionState.Failure](-connection-state/-failure/index.md). If all goes well, eventually
the state will be updated to [ConnectionState.Authenticating](-connection-state/-authenticating/index.md). A client should call either
[startAdvertising](./start-advertising.md) or [startDiscovering](start-discovering.md) to make a connection, not both. To initiate a
connection, one device must call [startAdvertising](./start-advertising.md) and the other [startDiscovering](start-discovering.md).

