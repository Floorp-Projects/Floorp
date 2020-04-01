[android-components](../../index.md) / [mozilla.components.lib.nearby](../index.md) / [NearbyConnectionObserver](index.md) / [onStateUpdated](./on-state-updated.md)

# onStateUpdated

`abstract fun onStateUpdated(connectionState: `[`ConnectionState`](../-nearby-connection/-connection-state/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/nearby/src/main/java/mozilla/components/lib/nearby/NearbyConnection.kt#L450)

Called whenever the connection's state is set. In the absence of failures, the
new state should differ from the prior state, but that is not guaranteed.

### Parameters

`connectionState` - the current state