[android-components](../../index.md) / [mozilla.components.browser.engine.gecko.webextension](../index.md) / [GeckoWebExtension](./index.md)

# GeckoWebExtension

`class GeckoWebExtension : `[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/webextension/GeckoWebExtension.kt#L12)

Gecko-based implementation of [WebExtension](../../mozilla.components.concept.engine.webextension/-web-extension/index.md), wrapping the native web
extension object provided by GeckoView.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `GeckoWebExtension(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, nativeExtension: `[`WebExtension`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/WebExtension.html)` = GeckoNativeWebExtension(url, id))`<br>`GeckoWebExtension(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, allowContentMessaging: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, nativeExtension: `[`WebExtension`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/WebExtension.html)` = GeckoNativeWebExtension(url, id, allowContentMessaging))`<br>Gecko-based implementation of [WebExtension](../../mozilla.components.concept.engine.webextension/-web-extension/index.md), wrapping the native web extension object provided by GeckoView. |

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
| [hasContentMessageHandler](has-content-message-handler.md) | `fun hasContentMessageHandler(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks whether there is an existing content message handler for the provided session and "application" name. |
| [registerBackgroundMessageHandler](register-background-message-handler.md) | `fun registerBackgroundMessageHandler(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, messageHandler: `[`MessageHandler`](../../mozilla.components.concept.engine.webextension/-message-handler/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers a [MessageHandler](../../mozilla.components.concept.engine.webextension/-message-handler/index.md) for message events from background scripts. |
| [registerContentMessageHandler](register-content-message-handler.md) | `fun registerContentMessageHandler(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, messageHandler: `[`MessageHandler`](../../mozilla.components.concept.engine.webextension/-message-handler/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers a [MessageHandler](../../mozilla.components.concept.engine.webextension/-message-handler/index.md) for message events from content scripts. |
