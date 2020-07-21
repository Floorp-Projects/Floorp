[android-components](../../../../index.md) / [mozilla.components.lib.nearby](../../../index.md) / [NearbyConnection](../../index.md) / [ConnectionState](../index.md) / [Authenticating](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`Authenticating(nearbyConnection: `[`NearbyConnection`](../../index.md)`, fallbackState: `[`ConnectionState`](../index.md)`, neighborId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, neighborName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, token: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`

This device is in the process of authenticating with a neighboring device. If both
devices accept the token, the next state will be [Connecting](../-connecting/index.md).

