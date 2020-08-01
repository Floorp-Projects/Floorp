[android-components](../../index.md) / [mozilla.components.feature.syncedtabs.controller](../index.md) / [SyncedTabsController](index.md) / [refreshSyncedTabs](./refresh-synced-tabs.md)

# refreshSyncedTabs

`abstract fun refreshSyncedTabs(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/syncedtabs/src/main/java/mozilla/components/feature/syncedtabs/controller/SyncedTabsController.kt#L24)

Requests for remote tabs and notifies the [SyncedTabsView](../../mozilla.components.feature.syncedtabs.view/-synced-tabs-view/index.md) when available with [SyncedTabsView.displaySyncedTabs](../../mozilla.components.feature.syncedtabs.view/-synced-tabs-view/display-synced-tabs.md)
otherwise notifies the appropriate error to [SyncedTabsView.onError](../../mozilla.components.feature.syncedtabs.view/-synced-tabs-view/on-error.md).

