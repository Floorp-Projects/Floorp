[android-components](../../index.md) / [mozilla.components.feature.syncedtabs.view](../index.md) / [SyncedTabsView](./index.md)

# SyncedTabsView

`interface SyncedTabsView` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/syncedtabs/src/main/java/mozilla/components/feature/syncedtabs/view/SyncedTabsView.kt#L14)

An interface for views that can display Firefox Sync "synced tabs" and related UI controls.

### Types

| Name | Summary |
|---|---|
| [ErrorType](-error-type/index.md) | `enum class ErrorType`<br>The various types of errors that can occur from syncing tabs. |
| [Listener](-listener/index.md) | `interface Listener`<br>An interface for notifying the listener of the [SyncedTabsView](./index.md). |

### Properties

| Name | Summary |
|---|---|
| [listener](listener.md) | `abstract var listener: `[`Listener`](-listener/index.md)`?` |

### Functions

| Name | Summary |
|---|---|
| [asView](as-view.md) | `open fun asView(): <ERROR CLASS>`<br>Casts this [SyncedTabsView](./index.md) interface to an actual Android [View](#) object. |
| [displaySyncedTabs](display-synced-tabs.md) | `abstract fun displaySyncedTabs(syncedTabs: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`SyncedDeviceTabs`](../../mozilla.components.browser.storage.sync/-synced-device-tabs/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>New tabs have been received to display. |
| [onError](on-error.md) | `abstract fun onError(error: `[`ErrorType`](-error-type/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>An error has occurred that may require various user-interactions based on the [ErrorType](-error-type/index.md). |
| [startLoading](start-loading.md) | `open fun startLoading(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>When tab syncing has started. |
| [stopLoading](stop-loading.md) | `open fun stopLoading(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>When tab syncing has completed. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
