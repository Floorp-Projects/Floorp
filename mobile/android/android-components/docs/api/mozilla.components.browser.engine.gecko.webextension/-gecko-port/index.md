[android-components](../../index.md) / [mozilla.components.browser.engine.gecko.webextension](../index.md) / [GeckoPort](./index.md)

# GeckoPort

`class GeckoPort : `[`Port`](../../mozilla.components.concept.engine.webextension/-port/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/webextension/GeckoWebExtension.kt#L378)

Gecko-based implementation of [Port](../../mozilla.components.concept.engine.webextension/-port/index.md), wrapping the native port provided by GeckoView.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `GeckoPort(nativePort: `[`Port`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/WebExtension/Port.html)`, engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`? = null)`<br>Gecko-based implementation of [Port](../../mozilla.components.concept.engine.webextension/-port/index.md), wrapping the native port provided by GeckoView. |

### Inherited Properties

| Name | Summary |
|---|---|
| [engineSession](../../mozilla.components.concept.engine.webextension/-port/engine-session.md) | `val engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`?` |

### Functions

| Name | Summary |
|---|---|
| [disconnect](disconnect.md) | `fun disconnect(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Disconnects this port. |
| [name](name.md) | `fun name(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Returns the name of this port. |
| [postMessage](post-message.md) | `fun postMessage(message: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sends a message to this port. |
| [senderUrl](sender-url.md) | `fun senderUrl(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Returns the URL of the port sender. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
