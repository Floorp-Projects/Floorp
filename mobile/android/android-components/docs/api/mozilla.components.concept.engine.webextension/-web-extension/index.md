[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [WebExtension](./index.md)

# WebExtension

`abstract class WebExtension` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtension.kt#L18)

Represents a browser extension based on the WebExtension API:
https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WebExtension(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>Represents a browser extension based on the WebExtension API: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions |

### Properties

| Name | Summary |
|---|---|
| [id](id.md) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the unique ID of this extension. |
| [url](url.md) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the url pointing to a resources path for locating the extension within the APK file e.g. resource://android/assets/extensions/my_web_ext. |

### Functions

| Name | Summary |
|---|---|
| [disconnectPort](disconnect-port.md) | `abstract fun disconnectPort(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Disconnect a [Port](../-port/index.md) of the provided [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md). This method has no effect if there's no connected port with the given name. |
| [getConnectedPort](get-connected-port.md) | `abstract fun getConnectedPort(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`? = null): `[`Port`](../-port/index.md)`?`<br>Returns a connected port with the given name and for the provided [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md), if one exists. |
| [hasContentMessageHandler](has-content-message-handler.md) | `abstract fun hasContentMessageHandler(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks whether there is an existing content message handler for the provided session and "application" name. |
| [registerBackgroundMessageHandler](register-background-message-handler.md) | `abstract fun registerBackgroundMessageHandler(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, messageHandler: `[`MessageHandler`](../-message-handler/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers a [MessageHandler](../-message-handler/index.md) for message events from background scripts. |
| [registerContentMessageHandler](register-content-message-handler.md) | `abstract fun registerContentMessageHandler(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, messageHandler: `[`MessageHandler`](../-message-handler/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers a [MessageHandler](../-message-handler/index.md) for message events from content scripts. |

### Inheritors

| Name | Summary |
|---|---|
| [GeckoWebExtension](../../mozilla.components.browser.engine.gecko.webextension/-gecko-web-extension/index.md) | `class GeckoWebExtension : `[`WebExtension`](./index.md)<br>Gecko-based implementation of [WebExtension](./index.md), wrapping the native web extension object provided by GeckoView. |
