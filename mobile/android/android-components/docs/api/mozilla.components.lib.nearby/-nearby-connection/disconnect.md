[android-components](../../index.md) / [mozilla.components.lib.nearby](../index.md) / [NearbyConnection](index.md) / [disconnect](./disconnect.md)

# disconnect

`fun disconnect(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/nearby/src/main/java/mozilla/components/lib/nearby/NearbyConnection.kt#L409)

Breaks any connections to neighboring devices. This also stops advertising and
discovering. The state will be updated to [ConnectionState.Isolated](-connection-state/-isolated.md). It is
important to call this when the connection is no longer needed because of
a [leak in the GATT client](http://bit.ly/33VP1gn).

