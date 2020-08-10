[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [WebExtension](index.md) / [getConnectedPort](./get-connected-port.md)

# getConnectedPort

`abstract fun getConnectedPort(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`? = null): `[`Port`](../-port/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtension.kt#L77)

Returns a connected port with the given name and for the provided
[EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md), if one exists.

### Parameters

`name` - the name as provided to connectNative.

`session` - (optional) session to check for, null if port is from a
background script.

**Return**
a matching port, or null if none is connected.

