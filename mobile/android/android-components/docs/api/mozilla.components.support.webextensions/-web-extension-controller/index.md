[android-components](../../index.md) / [mozilla.components.support.webextensions](../index.md) / [WebExtensionController](./index.md)

# WebExtensionController

`class WebExtensionController` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/webextensions/src/main/java/mozilla/components/support/webextensions/WebExtensionController.kt#L23)

Provides functionality to feature modules that need to interact with a web extension.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WebExtensionController(extensionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, extensionUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>Provides functionality to feature modules that need to interact with a web extension. |

### Functions

| Name | Summary |
|---|---|
| [disconnectPort](disconnect-port.md) | `fun disconnectPort(engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`?, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = extensionId): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Disconnects the port of the provided session. |
| [install](install.md) | `fun install(engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Makes sure the web extension is installed in the provided engine. If a content message handler was registered (see [registerContentMessageHandler](register-content-message-handler.md)) before install completed, registration will happen upon successful installation. |
| [portConnected](port-connected.md) | `fun portConnected(engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`?, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = extensionId): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks whether or not a port is connected for the provided session. |
| [registerContentMessageHandler](register-content-message-handler.md) | `fun registerContentMessageHandler(engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, messageHandler: `[`MessageHandler`](../../mozilla.components.concept.engine.webextension/-message-handler/index.md)`, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = extensionId): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers a content message handler for the provided session. Currently only one handler can be registered per session. An existing handler will be replaced and there is no need to unregister. |
| [sendContentMessage](send-content-message.md) | `fun sendContentMessage(msg: <ERROR CLASS>, engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`?, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = extensionId): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sends a content message to the provided session. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [installedExtensions](installed-extensions.md) | `val installedExtensions: `[`ConcurrentHashMap`](https://developer.android.com/reference/java/util/concurrent/ConcurrentHashMap.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`>` |
