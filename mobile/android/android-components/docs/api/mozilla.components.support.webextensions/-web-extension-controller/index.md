[android-components](../../index.md) / [mozilla.components.support.webextensions](../index.md) / [WebExtensionController](./index.md)

# WebExtensionController

`class WebExtensionController` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/webextensions/src/main/java/mozilla/components/support/webextensions/WebExtensionController.kt#L25)

Provides functionality to feature modules that need to interact with a web extension.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WebExtensionController(extensionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, extensionUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, messagingId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>Provides functionality to feature modules that need to interact with a web extension. |

### Functions

| Name | Summary |
|---|---|
| [disconnectPort](disconnect-port.md) | `fun disconnectPort(engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`?, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = messagingId): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Disconnects the port of the provided session. |
| [install](install.md) | `fun install(runtime: `[`WebExtensionRuntime`](../../mozilla.components.concept.engine.webextension/-web-extension-runtime/index.md)`, onSuccess: (`[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }, onError: (`[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { _ -> }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Makes sure the web extension is installed in the provided runtime. If a content message handler was registered (see [registerContentMessageHandler](register-content-message-handler.md)) before install completed, registration will happen upon successful installation. |
| [portConnected](port-connected.md) | `fun portConnected(engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`?, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = messagingId): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks whether or not a port is connected for the provided session. |
| [registerBackgroundMessageHandler](register-background-message-handler.md) | `fun registerBackgroundMessageHandler(messageHandler: `[`MessageHandler`](../../mozilla.components.concept.engine.webextension/-message-handler/index.md)`, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = messagingId): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers a background message handler for this extension. An existing handler will be replaced and there is no need to unregister. |
| [registerContentMessageHandler](register-content-message-handler.md) | `fun registerContentMessageHandler(engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, messageHandler: `[`MessageHandler`](../../mozilla.components.concept.engine.webextension/-message-handler/index.md)`, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = messagingId): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers a content message handler for the provided session. Currently only one handler can be registered per session. An existing handler will be replaced and there is no need to unregister. |
| [sendBackgroundMessage](send-background-message.md) | `fun sendBackgroundMessage(msg: <ERROR CLASS>, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = messagingId): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sends a background message to the provided extension. |
| [sendContentMessage](send-content-message.md) | `fun sendContentMessage(msg: <ERROR CLASS>, engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`?, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = messagingId): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sends a content message to the provided session. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [installedExtensions](installed-extensions.md) | `val installedExtensions: `[`ConcurrentHashMap`](http://docs.oracle.com/javase/7/docs/api/java/util/concurrent/ConcurrentHashMap.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`>` |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
