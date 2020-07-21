[android-components](../../../index.md) / [mozilla.components.feature.syncedtabs.view](../../index.md) / [SyncedTabsView](../index.md) / [Listener](./index.md)

# Listener

`interface Listener` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/syncedtabs/src/main/java/mozilla/components/feature/syncedtabs/view/SyncedTabsView.kt#L45)

An interface for notifying the listener of the [SyncedTabsView](../index.md).

### Functions

| Name | Summary |
|---|---|
| [onRefresh](on-refresh.md) | `abstract fun onRefresh(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Invoked when receiving a request to refresh the synced tabs. |
| [onTabClicked](on-tab-clicked.md) | `abstract fun onTabClicked(tab: `[`Tab`](../../../mozilla.components.browser.storage.sync/-tab/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Invoked when a tab has been selected. |

### Inheritors

| Name | Summary |
|---|---|
| [SyncedTabsInteractor](../../../mozilla.components.feature.syncedtabs.interactor/-synced-tabs-interactor/index.md) | `interface SyncedTabsInteractor : `[`Listener`](./index.md)`, `[`LifecycleAwareFeature`](../../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)<br>An interactor that handles events from [SyncedTabsView.Listener](./index.md). |
