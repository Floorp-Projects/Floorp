[android-components](../../../index.md) / [mozilla.components.lib.nearby](../../index.md) / [NearbyConnection](../index.md) / [ConnectionState](index.md) / [Advertising](./-advertising.md)

# Advertising

`object Advertising : `[`ConnectionState`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/nearby/src/main/java/mozilla/components/lib/nearby/NearbyConnection.kt#L115)

This device is advertising its presence. If it is discovered by another device,
the next state will be [Authenticating](-authenticating/index.md).

### Inherited Properties

| Name | Summary |
|---|---|
| [name](name.md) | `val name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The name of the state, which is the same as the name of the concrete class (e.g., "Isolated" for [Isolated](-isolated.md)). |
