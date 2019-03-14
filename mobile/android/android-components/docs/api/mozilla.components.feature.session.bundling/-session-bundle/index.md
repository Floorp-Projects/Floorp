[android-components](../../index.md) / [mozilla.components.feature.session.bundling](../index.md) / [SessionBundle](./index.md)

# SessionBundle

`interface SessionBundle` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session-bundling/src/main/java/mozilla/components/feature/session/bundling/SessionBundle.kt#L13)

A bundle of sessions and their state.

### Properties

| Name | Summary |
|---|---|
| [id](id.md) | `abstract val id: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`?`<br>A unique ID identifying this bundle. Can be `null` if this is a new Bundle that has not been saved to disk yet. |
| [lastSavedAt](last-saved-at.md) | `abstract val lastSavedAt: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>Timestamp of the last time this bundle was saved (in milliseconds) |
| [urls](urls.md) | `abstract val urls: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>List of URLs saved in this bundle. |

### Functions

| Name | Summary |
|---|---|
| [restoreSnapshot](restore-snapshot.md) | `abstract fun restoreSnapshot(): `[`Snapshot`](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md)`?`<br>Restore a [SessionManager.Snapshot](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md) from this bundle. The returned snapshot can be used with [SessionManager](../../mozilla.components.browser.session/-session-manager/index.md) to restore the sessions and their state. |
