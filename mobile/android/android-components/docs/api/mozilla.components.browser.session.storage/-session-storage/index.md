[android-components](../../index.md) / [mozilla.components.browser.session.storage](../index.md) / [SessionStorage](./index.md)

# SessionStorage

`class SessionStorage : `[`Storage`](../-auto-save/-storage/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/storage/SessionStorage.kt#L27)

Session storage for persisting the state of a [SessionManager](../../mozilla.components.browser.session/-session-manager/index.md) to disk (browser and engine session states).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SessionStorage(context: <ERROR CLASS>, engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`)`<br>Session storage for persisting the state of a [SessionManager](../../mozilla.components.browser.session/-session-manager/index.md) to disk (browser and engine session states). |

### Functions

| Name | Summary |
|---|---|
| [autoSave](auto-save.md) | `fun autoSave(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, interval: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = AutoSave.DEFAULT_INTERVAL_MILLISECONDS, unit: `[`TimeUnit`](http://docs.oracle.com/javase/7/docs/api/java/util/concurrent/TimeUnit.html)` = TimeUnit.MILLISECONDS): `[`AutoSave`](../-auto-save/index.md)<br>Starts configuring automatic saving of the state. |
| [clear](clear.md) | `fun clear(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Clears the state saved on disk. |
| [restore](restore.md) | `fun restore(): `[`Snapshot`](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md)`?`<br>Reads the saved state from disk. Returns null if no state was found on disk or if reading the file failed. |
| [save](save.md) | `fun save(snapshot: `[`Snapshot`](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Saves the given state to disk. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
