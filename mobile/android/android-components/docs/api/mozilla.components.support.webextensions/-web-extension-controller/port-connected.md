[android-components](../../index.md) / [mozilla.components.support.webextensions](../index.md) / [WebExtensionController](index.md) / [portConnected](./port-connected.md)

# portConnected

`fun portConnected(engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`?, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = messagingId): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/webextensions/src/main/java/mozilla/components/support/webextensions/WebExtensionController.kt#L152)

Checks whether or not a port is connected for the provided session.

### Parameters

`engineSession` - the session the port should be connected to or null for a port to a background script.

`name` - (optional) name of the port, defaults to the provided [messagingId](#).