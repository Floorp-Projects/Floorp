[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [WebExtension](index.md) / [disconnectPort](./disconnect-port.md)

# disconnectPort

`abstract fun disconnectPort(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtension.kt#L83)

Disconnect a [Port](../-port/index.md) of the provided [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md). This method has
no effect if there's no connected port with the given name.

### Parameters

`name` - the name as provided to connectNative, see
[registerContentMessageHandler](register-content-message-handler.md) and [registerBackgroundMessageHandler](register-background-message-handler.md).

`session` - (options) session for which ports should disconnected,
null if port is from a background script.