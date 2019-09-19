[android-components](../../index.md) / [mozilla.components.support.webextensions](../index.md) / [WebExtensionController](index.md) / [disconnectPort](./disconnect-port.md)

# disconnectPort

`fun disconnectPort(engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`?, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = extensionId): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/webextensions/src/main/java/mozilla/components/support/webextensions/WebExtensionController.kt#L114)

Disconnects the port of the provided session.

### Parameters

`engineSession` - the session the port is connected to.

`name` - (optional) name of the port, defaults to the provided extensionId.