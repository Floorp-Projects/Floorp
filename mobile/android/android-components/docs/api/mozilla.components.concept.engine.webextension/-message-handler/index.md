[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [MessageHandler](./index.md)

# MessageHandler

`interface MessageHandler` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtension.kt#L96)

A handler for all messaging related events, usable for both content and
background scripts.

[Port](../-port/index.md)s are exposed to consumers (higher level components) because
how ports are used, how many there are and how messages map to it
is feature-specific and depends on the design of the web extension.
Therefore it makes most sense to let the extensions (higher-level
features) deal with the management of ports.

### Functions

| Name | Summary |
|---|---|
| [onMessage](on-message.md) | `open fun onMessage(message: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`, source: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`?): `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`?`<br>Invoked when a message was received as a result of a `browser.runtime.sendNativeMessage` call in JavaScript. |
| [onPortConnected](on-port-connected.md) | `open fun onPortConnected(port: `[`Port`](../-port/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Invoked when a [Port](../-port/index.md) was connected as a result of a `browser.runtime.connectNative` call in JavaScript. |
| [onPortDisconnected](on-port-disconnected.md) | `open fun onPortDisconnected(port: `[`Port`](../-port/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Invoked when a [Port](../-port/index.md) was disconnected or the corresponding session was destroyed. |
| [onPortMessage](on-port-message.md) | `open fun onPortMessage(message: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`, port: `[`Port`](../-port/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Invoked when a message was received on the provided port. |
