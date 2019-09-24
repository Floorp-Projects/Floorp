[android-components](../../index.md) / [mozilla.components.browser.engine.gecko.webextension](../index.md) / [GeckoWebExtension](./index.md)

# GeckoWebExtension

`class GeckoWebExtension : `[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/webextension/GeckoWebExtension.kt#L20)

Gecko-based implementation of [WebExtension](../../mozilla.components.concept.engine.webextension/-web-extension/index.md), wrapping the native web
extension object provided by GeckoView.

### Types

| Name | Summary |
|---|---|
| [PortId](-port-id/index.md) | `data class PortId`<br>Uniquely identifies a port using its name and the session it was opened for. Ports connected from background scripts will have a null session. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `GeckoWebExtension(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, allowContentMessaging: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, nativeExtension: `[`WebExtension`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/WebExtension.html)` = GeckoNativeWebExtension(
        url,
        id,
        createWebExtensionFlags(allowContentMessaging)
    ), connectedPorts: `[`MutableMap`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-mutable-map/index.html)`<`[`PortId`](-port-id/index.md)`, `[`Port`](../../mozilla.components.concept.engine.webextension/-port/index.md)`> = mutableMapOf())`<br>Gecko-based implementation of [WebExtension](../../mozilla.components.concept.engine.webextension/-web-extension/index.md), wrapping the native web extension object provided by GeckoView. |

### Properties

| Name | Summary |
|---|---|
| [nativeExtension](native-extension.md) | `val nativeExtension: `[`WebExtension`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/WebExtension.html) |

### Inherited Properties

| Name | Summary |
|---|---|
| [id](../../mozilla.components.concept.engine.webextension/-web-extension/id.md) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the unique ID of this extension. |
| [url](../../mozilla.components.concept.engine.webextension/-web-extension/url.md) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the url pointing to a resources path for locating the extension within the APK file e.g. resource://android/assets/extensions/my_web_ext. |

### Functions

| Name | Summary |
|---|---|
| [disconnectPort](disconnect-port.md) | `fun disconnectPort(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [WebExtension.disconnectPort](../../mozilla.components.concept.engine.webextension/-web-extension/disconnect-port.md). |
| [getConnectedPort](get-connected-port.md) | `fun getConnectedPort(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`?): `[`Port`](../../mozilla.components.concept.engine.webextension/-port/index.md)`?`<br>See [WebExtension.getConnectedPort](../../mozilla.components.concept.engine.webextension/-web-extension/get-connected-port.md). |
| [hasContentMessageHandler](has-content-message-handler.md) | `fun hasContentMessageHandler(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>See [WebExtension.hasContentMessageHandler](../../mozilla.components.concept.engine.webextension/-web-extension/has-content-message-handler.md). |
| [registerBackgroundMessageHandler](register-background-message-handler.md) | `fun registerBackgroundMessageHandler(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, messageHandler: `[`MessageHandler`](../../mozilla.components.concept.engine.webextension/-message-handler/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [WebExtension.registerBackgroundMessageHandler](../../mozilla.components.concept.engine.webextension/-web-extension/register-background-message-handler.md). |
| [registerContentMessageHandler](register-content-message-handler.md) | `fun registerContentMessageHandler(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, messageHandler: `[`MessageHandler`](../../mozilla.components.concept.engine.webextension/-message-handler/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [WebExtension.registerContentMessageHandler](../../mozilla.components.concept.engine.webextension/-web-extension/register-content-message-handler.md). |
