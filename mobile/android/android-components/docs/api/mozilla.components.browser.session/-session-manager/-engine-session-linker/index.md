[android-components](../../../index.md) / [mozilla.components.browser.session](../../index.md) / [SessionManager](../index.md) / [EngineSessionLinker](./index.md)

# EngineSessionLinker

`class EngineSessionLinker` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/SessionManager.kt#L39)

This class only exists for migrating from browser-session
to browser-state. We need a way to dispatch the corresponding browser
actions when an engine session is linked and unlinked.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `EngineSessionLinker(store: `[`BrowserStore`](../../../mozilla.components.browser.state.store/-browser-store/index.md)`?)`<br>This class only exists for migrating from browser-session to browser-state. We need a way to dispatch the corresponding browser actions when an engine session is linked and unlinked. |

### Functions

| Name | Summary |
|---|---|
| [link](link.md) | `fun link(session: `[`Session`](../../-session/index.md)`, engineSession: `[`EngineSession`](../../../mozilla.components.concept.engine/-engine-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [unlink](unlink.md) | `fun unlink(session: `[`Session`](../../-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
