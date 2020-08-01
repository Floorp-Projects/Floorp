[android-components](../../index.md) / [mozilla.components.browser.engine.gecko.webextension](../index.md) / [GeckoWebExtension](./index.md)

# GeckoWebExtension

`class GeckoWebExtension : `[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/webextension/GeckoWebExtension.kt#L33)

Gecko-based implementation of [WebExtension](../../mozilla.components.concept.engine.webextension/-web-extension/index.md), wrapping the native web
extension object provided by GeckoView.

### Types

| Name | Summary |
|---|---|
| [PortId](-port-id/index.md) | `data class PortId`<br>Uniquely identifies a port using its name and the session it was opened for. Ports connected from background scripts will have a null session. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `GeckoWebExtension(nativeExtension: `[`WebExtension`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/WebExtension.html)`, runtime: `[`GeckoRuntime`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoRuntime.html)`)`<br>Gecko-based implementation of [WebExtension](../../mozilla.components.concept.engine.webextension/-web-extension/index.md), wrapping the native web extension object provided by GeckoView. |

### Properties

| Name | Summary |
|---|---|
| [nativeExtension](native-extension.md) | `val nativeExtension: `[`WebExtension`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/WebExtension.html) |
| [runtime](runtime.md) | `val runtime: `[`GeckoRuntime`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoRuntime.html) |

### Inherited Properties

| Name | Summary |
|---|---|
| [id](../../mozilla.components.concept.engine.webextension/-web-extension/id.md) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the unique ID of this extension. |
| [supportActions](../../mozilla.components.concept.engine.webextension/-web-extension/support-actions.md) | `val supportActions: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>whether or not browser and page actions are handled when received from the web extension |
| [url](../../mozilla.components.concept.engine.webextension/-web-extension/url.md) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the url pointing to a resources path for locating the extension within the APK file e.g. resource://android/assets/extensions/my_web_ext. |

### Functions

| Name | Summary |
|---|---|
| [disconnectPort](disconnect-port.md) | `fun disconnectPort(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [WebExtension.disconnectPort](../../mozilla.components.concept.engine.webextension/-web-extension/disconnect-port.md). |
| [getConnectedPort](get-connected-port.md) | `fun getConnectedPort(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`?): `[`Port`](../../mozilla.components.concept.engine.webextension/-port/index.md)`?`<br>See [WebExtension.getConnectedPort](../../mozilla.components.concept.engine.webextension/-web-extension/get-connected-port.md). |
| [getMetadata](get-metadata.md) | `fun getMetadata(): `[`Metadata`](../../mozilla.components.concept.engine.webextension/-metadata/index.md)`?`<br>See [WebExtension.getMetadata](../../mozilla.components.concept.engine.webextension/-web-extension/get-metadata.md). |
| [hasActionHandler](has-action-handler.md) | `fun hasActionHandler(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>See [WebExtension.hasActionHandler](../../mozilla.components.concept.engine.webextension/-web-extension/has-action-handler.md). |
| [hasContentMessageHandler](has-content-message-handler.md) | `fun hasContentMessageHandler(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>See [WebExtension.hasContentMessageHandler](../../mozilla.components.concept.engine.webextension/-web-extension/has-content-message-handler.md). |
| [hasTabHandler](has-tab-handler.md) | `fun hasTabHandler(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>See [WebExtension.hasTabHandler](../../mozilla.components.concept.engine.webextension/-web-extension/has-tab-handler.md). |
| [isAllowedInPrivateBrowsing](is-allowed-in-private-browsing.md) | `fun isAllowedInPrivateBrowsing(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks whether or not this extension is allowed in private browsing. |
| [isBuiltIn](is-built-in.md) | `fun isBuiltIn(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks whether or not this extension is built-in (packaged with the APK file) or coming from an external source. |
| [isEnabled](is-enabled.md) | `fun isEnabled(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks whether or not this extension is enabled. |
| [registerActionHandler](register-action-handler.md) | `fun registerActionHandler(actionHandler: `[`ActionHandler`](../../mozilla.components.concept.engine.webextension/-action-handler/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>`fun registerActionHandler(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, actionHandler: `[`ActionHandler`](../../mozilla.components.concept.engine.webextension/-action-handler/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [WebExtension.registerActionHandler](../../mozilla.components.concept.engine.webextension/-web-extension/register-action-handler.md). |
| [registerBackgroundMessageHandler](register-background-message-handler.md) | `fun registerBackgroundMessageHandler(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, messageHandler: `[`MessageHandler`](../../mozilla.components.concept.engine.webextension/-message-handler/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [WebExtension.registerBackgroundMessageHandler](../../mozilla.components.concept.engine.webextension/-web-extension/register-background-message-handler.md). |
| [registerContentMessageHandler](register-content-message-handler.md) | `fun registerContentMessageHandler(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, messageHandler: `[`MessageHandler`](../../mozilla.components.concept.engine.webextension/-message-handler/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [WebExtension.registerContentMessageHandler](../../mozilla.components.concept.engine.webextension/-web-extension/register-content-message-handler.md). |
| [registerTabHandler](register-tab-handler.md) | `fun registerTabHandler(tabHandler: `[`TabHandler`](../../mozilla.components.concept.engine.webextension/-tab-handler/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>`fun registerTabHandler(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, tabHandler: `[`TabHandler`](../../mozilla.components.concept.engine.webextension/-tab-handler/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [WebExtension.registerTabHandler](../../mozilla.components.concept.engine.webextension/-web-extension/register-tab-handler.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [isUnsupported](../../mozilla.components.concept.engine.webextension/is-unsupported.md) | `fun `[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`.isUnsupported(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns whether or not the extension is disabled because it is unsupported. |
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
