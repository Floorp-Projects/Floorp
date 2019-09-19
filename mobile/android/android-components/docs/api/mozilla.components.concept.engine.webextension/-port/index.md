[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [Port](./index.md)

# Port

`abstract class Port` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtension.kt#L141)

Represents a port for exchanging messages:
https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/runtime/Port

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Port(engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`? = null)`<br>Represents a port for exchanging messages: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/runtime/Port |

### Properties

| Name | Summary |
|---|---|
| [engineSession](engine-session.md) | `val engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`?` |

### Functions

| Name | Summary |
|---|---|
| [disconnect](disconnect.md) | `abstract fun disconnect(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Disconnects this port. |
| [name](name.md) | `abstract fun name(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Returns the name of this port. |
| [postMessage](post-message.md) | `abstract fun postMessage(message: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sends a message to this port. |

### Inheritors

| Name | Summary |
|---|---|
| [GeckoPort](../../mozilla.components.browser.engine.gecko.webextension/-gecko-port/index.md) | `class GeckoPort : `[`Port`](./index.md)<br>Gecko-based implementation of [Port](./index.md), wrapping the native port provided by GeckoView. |
