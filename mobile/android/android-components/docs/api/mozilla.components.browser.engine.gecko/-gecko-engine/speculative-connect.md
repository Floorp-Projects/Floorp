[android-components](../../index.md) / [mozilla.components.browser.engine.gecko](../index.md) / [GeckoEngine](index.md) / [speculativeConnect](./speculative-connect.md)

# speculativeConnect

`fun speculativeConnect(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/GeckoEngine.kt#L123)

Overrides [Engine.speculativeConnect](../../mozilla.components.concept.engine/-engine/speculative-connect.md)

Opens a speculative connection to the host of [url](speculative-connect.md#mozilla.components.browser.engine.gecko.GeckoEngine$speculativeConnect(kotlin.String)/url).

This is useful if an app thinks it may be making a request to that host in the near future. If no request
is made, the connection will be cleaned up after an unspecified.

