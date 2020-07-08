[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [WebExtension](./index.md)

# WebExtension

`abstract class WebExtension` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtension.kt#L22)

Represents a browser extension based on the WebExtension API:
https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WebExtension(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, supportActions: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`)`<br>Represents a browser extension based on the WebExtension API: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions |

### Properties

| Name | Summary |
|---|---|
| [id](id.md) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the unique ID of this extension. |
| [supportActions](support-actions.md) | `val supportActions: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>whether or not browser and page actions are handled when received from the web extension |
| [url](url.md) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the url pointing to a resources path for locating the extension within the APK file e.g. resource://android/assets/extensions/my_web_ext. |

### Functions

| Name | Summary |
|---|---|
| [disconnectPort](disconnect-port.md) | `abstract fun disconnectPort(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Disconnect a [Port](../-port/index.md) of the provided [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md). This method has no effect if there's no connected port with the given name. |
| [getConnectedPort](get-connected-port.md) | `abstract fun getConnectedPort(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`? = null): `[`Port`](../-port/index.md)`?`<br>Returns a connected port with the given name and for the provided [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md), if one exists. |
| [getMetadata](get-metadata.md) | `abstract fun getMetadata(): `[`Metadata`](../-metadata/index.md)`?`<br>Returns additional information about this extension. |
| [hasActionHandler](has-action-handler.md) | `abstract fun hasActionHandler(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks whether there is an existing action handler for the provided session. |
| [hasContentMessageHandler](has-content-message-handler.md) | `abstract fun hasContentMessageHandler(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks whether there is an existing content message handler for the provided session and "application" name. |
| [hasTabHandler](has-tab-handler.md) | `abstract fun hasTabHandler(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks whether there is an existing tab handler for the provided session. |
| [isAllowedInPrivateBrowsing](is-allowed-in-private-browsing.md) | `abstract fun isAllowedInPrivateBrowsing(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks whether or not this extension is allowed in private browsing. |
| [isBuiltIn](is-built-in.md) | `open fun isBuiltIn(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks whether or not this extension is built-in (packaged with the APK file) or coming from an external source. |
| [isEnabled](is-enabled.md) | `abstract fun isEnabled(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks whether or not this extension is enabled. |
| [registerActionHandler](register-action-handler.md) | `abstract fun registerActionHandler(actionHandler: `[`ActionHandler`](../-action-handler/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers an [ActionHandler](../-action-handler/index.md) for this web extension. The handler will be invoked whenever browser and page action defaults change. To listen for session-specific overrides see registerActionHandler( EngineSession, ActionHandler).`abstract fun registerActionHandler(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, actionHandler: `[`ActionHandler`](../-action-handler/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers an [ActionHandler](../-action-handler/index.md) for the provided [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md). The handler will be invoked whenever browser and page action overrides are received for the provided session. |
| [registerBackgroundMessageHandler](register-background-message-handler.md) | `abstract fun registerBackgroundMessageHandler(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, messageHandler: `[`MessageHandler`](../-message-handler/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers a [MessageHandler](../-message-handler/index.md) for message events from background scripts. |
| [registerContentMessageHandler](register-content-message-handler.md) | `abstract fun registerContentMessageHandler(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, messageHandler: `[`MessageHandler`](../-message-handler/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers a [MessageHandler](../-message-handler/index.md) for message events from content scripts. |
| [registerTabHandler](register-tab-handler.md) | `abstract fun registerTabHandler(tabHandler: `[`TabHandler`](../-tab-handler/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers a [TabHandler](../-tab-handler/index.md) for this web extension. This handler will be invoked whenever a web extension wants to open a new tab. To listen for session-specific events (such as [TabHandler.onCloseTab](../-tab-handler/on-close-tab.md)) use registerTabHandler(EngineSession, TabHandler) instead.`abstract fun registerTabHandler(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, tabHandler: `[`TabHandler`](../-tab-handler/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers a [TabHandler](../-tab-handler/index.md) for the provided [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md). The handler will be invoked whenever an existing tab should be closed or updated. |

### Extension Functions

| Name | Summary |
|---|---|
| [isUnsupported](../is-unsupported.md) | `fun `[`WebExtension`](./index.md)`.isUnsupported(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns whether or not the extension is disabled because it is unsupported. |
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [GeckoWebExtension](../../mozilla.components.browser.engine.gecko.webextension/-gecko-web-extension/index.md) | `class GeckoWebExtension : `[`WebExtension`](./index.md)<br>Gecko-based implementation of [WebExtension](./index.md), wrapping the native web extension object provided by GeckoView. |
