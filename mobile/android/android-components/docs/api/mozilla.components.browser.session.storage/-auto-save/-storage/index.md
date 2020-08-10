[android-components](../../../index.md) / [mozilla.components.browser.session.storage](../../index.md) / [AutoSave](../index.md) / [Storage](./index.md)

# Storage

`interface Storage` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/storage/AutoSave.kt#L32)

### Functions

| Name | Summary |
|---|---|
| [save](save.md) | `abstract fun save(snapshot: `[`Snapshot`](../../../mozilla.components.browser.session/-session-manager/-snapshot/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Inheritors

| Name | Summary |
|---|---|
| [SessionStorage](../../-session-storage/index.md) | `class SessionStorage : `[`Storage`](./index.md)<br>Session storage for persisting the state of a [SessionManager](../../../mozilla.components.browser.session/-session-manager/index.md) to disk (browser and engine session states). |
