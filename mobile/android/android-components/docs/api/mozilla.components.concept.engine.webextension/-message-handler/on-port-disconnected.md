[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [MessageHandler](index.md) / [onPortDisconnected](./on-port-disconnected.md)

# onPortDisconnected

`open fun onPortDisconnected(port: `[`Port`](../-port/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtension.kt#L240)

Invoked when a [Port](../-port/index.md) was disconnected or the corresponding session was
destroyed.

### Parameters

`port` - the disconnected port.