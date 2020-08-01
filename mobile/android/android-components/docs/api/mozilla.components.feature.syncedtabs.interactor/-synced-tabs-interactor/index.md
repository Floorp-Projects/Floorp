[android-components](../../index.md) / [mozilla.components.feature.syncedtabs.interactor](../index.md) / [SyncedTabsInteractor](./index.md)

# SyncedTabsInteractor

`interface SyncedTabsInteractor : `[`Listener`](../../mozilla.components.feature.syncedtabs.view/-synced-tabs-view/-listener/index.md)`, `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/syncedtabs/src/main/java/mozilla/components/feature/syncedtabs/interactor/SyncedTabsInteractor.kt#L15)

An interactor that handles events from [SyncedTabsView.Listener](../../mozilla.components.feature.syncedtabs.view/-synced-tabs-view/-listener/index.md).

### Properties

| Name | Summary |
|---|---|
| [controller](controller.md) | `abstract val controller: `[`SyncedTabsController`](../../mozilla.components.feature.syncedtabs.controller/-synced-tabs-controller/index.md) |
| [tabClicked](tab-clicked.md) | `abstract val tabClicked: (`[`Tab`](../../mozilla.components.browser.storage.sync/-tab/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [view](view.md) | `abstract val view: `[`SyncedTabsView`](../../mozilla.components.feature.syncedtabs.view/-synced-tabs-view/index.md) |

### Inherited Functions

| Name | Summary |
|---|---|
| [onRefresh](../../mozilla.components.feature.syncedtabs.view/-synced-tabs-view/-listener/on-refresh.md) | `abstract fun onRefresh(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Invoked when receiving a request to refresh the synced tabs. |
| [onTabClicked](../../mozilla.components.feature.syncedtabs.view/-synced-tabs-view/-listener/on-tab-clicked.md) | `abstract fun onTabClicked(tab: `[`Tab`](../../mozilla.components.browser.storage.sync/-tab/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Invoked when a tab has been selected. |
| [start](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/start.md) | `abstract fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [stop](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/stop.md) | `abstract fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
