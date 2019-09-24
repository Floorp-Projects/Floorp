[android-components](../../../index.md) / [mozilla.components.browser.engine.gecko.webextension](../../index.md) / [GeckoWebExtension](../index.md) / [PortId](./index.md)

# PortId

`data class PortId` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/webextension/GeckoWebExtension.kt#L37)

Uniquely identifies a port using its name and the session it
was opened for. Ports connected from background scripts will
have a null session.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `PortId(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, session: `[`EngineSession`](../../../mozilla.components.concept.engine/-engine-session/index.md)`? = null)`<br>Uniquely identifies a port using its name and the session it was opened for. Ports connected from background scripts will have a null session. |

### Properties

| Name | Summary |
|---|---|
| [name](name.md) | `val name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [session](session.md) | `val session: `[`EngineSession`](../../../mozilla.components.concept.engine/-engine-session/index.md)`?` |
