[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [Engine](index.md) / [speculativeConnect](./speculative-connect.md)

# speculativeConnect

`abstract fun speculativeConnect(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/Engine.kt#L108)

Opens a speculative connection to the host of [url](speculative-connect.md#mozilla.components.concept.engine.Engine$speculativeConnect(kotlin.String)/url).

This is useful if an app thinks it may be making a request to that host in the near future. If no request
is made, the connection will be cleaned up after an unspecified.

Not all [Engine](index.md) implementations may actually implement this.

