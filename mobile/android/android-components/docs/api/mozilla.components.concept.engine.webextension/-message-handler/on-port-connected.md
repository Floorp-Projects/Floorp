[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [MessageHandler](index.md) / [onPortConnected](./on-port-connected.md)

# onPortConnected

`open fun onPortConnected(port: `[`Port`](../-port/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtension.kt#L232)

Invoked when a [Port](../-port/index.md) was connected as a result of a
`browser.runtime.connectNative` call in JavaScript.

### Parameters

`port` - the connected port.