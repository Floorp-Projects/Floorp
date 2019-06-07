[android-components](../../index.md) / [mozilla.components.browser.engine.gecko.webextension](../index.md) / [GeckoPort](./index.md)

# GeckoPort

`class GeckoPort : `[`Port`](../../mozilla.components.concept.engine.webextension/-port/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/webextension/GeckoWebExtension.kt#L102)

Gecko-based implementation of [Port](../../mozilla.components.concept.engine.webextension/-port/index.md), wrapping the native port provided by GeckoView.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `GeckoPort(nativePort: <ERROR CLASS>, engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`? = null)`<br>Gecko-based implementation of [Port](../../mozilla.components.concept.engine.webextension/-port/index.md), wrapping the native port provided by GeckoView. |

### Inherited Properties

| Name | Summary |
|---|---|
| [engineSession](../../mozilla.components.concept.engine.webextension/-port/engine-session.md) | `val engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`?` |

### Functions

| Name | Summary |
|---|---|
| [postMessage](post-message.md) | `fun postMessage(message: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sends a message to this port. |
