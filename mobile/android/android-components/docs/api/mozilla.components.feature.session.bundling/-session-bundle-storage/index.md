[android-components](../../index.md) / [mozilla.components.feature.session.bundling](../index.md) / [SessionBundleStorage](./index.md)

# SessionBundleStorage

`class SessionBundleStorage : `[`Storage`](../../mozilla.components.browser.session.storage/-auto-save/-storage/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session-bundling/src/main/java/mozilla/components/feature/session/bundling/SessionBundleStorage.kt#L36)

A [Session](../../mozilla.components.browser.session/-session/index.md) storage implementation that saves snapshots as a [SessionBundle](../-session-bundle/index.md).

### Parameters

`bundleLifetime` - The lifetime of a bundle controls whether a bundle will be restored or whether this bundle is
considered expired and a new bundle will be used.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SessionBundleStorage(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, bundleLifetime: `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, `[`TimeUnit`](https://developer.android.com/reference/java/util/concurrent/TimeUnit.html)`>)`<br>A [Session](../../mozilla.components.browser.session/-session/index.md) storage implementation that saves snapshots as a [SessionBundle](../-session-bundle/index.md). |

### Functions

| Name | Summary |
|---|---|
| [autoClose](auto-close.md) | `fun autoClose(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Automatically clears the current bundle and starts a new bundle if the lifetime has expired while the app was in the background. |
| [autoSave](auto-save.md) | `fun autoSave(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, interval: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = AutoSave.DEFAULT_INTERVAL_MILLISECONDS, unit: `[`TimeUnit`](https://developer.android.com/reference/java/util/concurrent/TimeUnit.html)` = TimeUnit.MILLISECONDS): `[`AutoSave`](../../mozilla.components.browser.session.storage/-auto-save/index.md)<br>Starts configuring automatic saving of the state. |
| [bundles](bundles.md) | `fun bundles(since: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = 0, limit: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 20): LiveData<`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`SessionBundle`](../-session-bundle/index.md)`>>`<br>Returns the last saved [SessionBundle](../-session-bundle/index.md) instances (up to [limit](bundles.md#mozilla.components.feature.session.bundling.SessionBundleStorage$bundles(kotlin.Long, kotlin.Int)/limit)) as a [LiveData](#) list. |
| [bundlesPaged](bundles-paged.md) | `fun bundlesPaged(since: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = 0): Factory<`[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, `[`SessionBundle`](../-session-bundle/index.md)`>`<br>Returns all saved [SessionBundle](../-session-bundle/index.md) instances as a [DataSource.Factory](#). |
| [current](current.md) | `fun current(): `[`SessionBundle`](../-session-bundle/index.md)`?`<br>Returns the currently used [SessionBundle](../-session-bundle/index.md) for saving [SessionManager.Snapshot](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md) instances. Or null if no bundle is in use currently. |
| [new](new.md) | `fun new(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Clear the currently used, active [SessionBundle](../-session-bundle/index.md) and use a new one the next time a [SessionManager.Snapshot](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md) is saved. |
| [remove](remove.md) | `fun remove(bundle: `[`SessionBundle`](../-session-bundle/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes the given [SessionBundle](../-session-bundle/index.md) permanently. If this is the active bundle then a new one will be created the next time a [SessionManager.Snapshot](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md) is saved. |
| [removeAll](remove-all.md) | `fun removeAll(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes all saved [SessionBundle](../-session-bundle/index.md) instances permanently. |
| [restore](restore.md) | `fun restore(): `[`SessionBundle`](../-session-bundle/index.md)`?`<br>Restores the last [SessionBundle](../-session-bundle/index.md) if there is one without expired lifetime. |
| [save](save.md) | `fun save(snapshot: `[`Snapshot`](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Saves the [SessionManager.Snapshot](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md) as a bundle. If a bundle was restored previously then this bundle will be updated with the data from the snapshot. If no bundle was restored a new bundle will be created. |
| [use](use.md) | `fun use(bundle: `[`SessionBundle`](../-session-bundle/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Explicitly uses the given [SessionBundle](../-session-bundle/index.md) (even if not active) to save [SessionManager.Snapshot](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md) instances to. |
