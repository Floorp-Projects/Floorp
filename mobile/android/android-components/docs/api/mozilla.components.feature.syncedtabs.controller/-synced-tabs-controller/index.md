[android-components](../../index.md) / [mozilla.components.feature.syncedtabs.controller](../index.md) / [SyncedTabsController](./index.md)

# SyncedTabsController

`interface SyncedTabsController` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/syncedtabs/src/main/java/mozilla/components/feature/syncedtabs/controller/SyncedTabsController.kt#L15)

A controller for making the appropriate request for remote tabs from [SyncedTabsProvider](../../mozilla.components.feature.syncedtabs.storage/-synced-tabs-provider/index.md) when the
[FxaAccountManager](../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md) account is in the appropriate state. The [SyncedTabsView](../../mozilla.components.feature.syncedtabs.view/-synced-tabs-view/index.md) can then be notified.

### Properties

| Name | Summary |
|---|---|
| [accountManager](account-manager.md) | `abstract val accountManager: `[`FxaAccountManager`](../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md) |
| [provider](provider.md) | `abstract val provider: `[`SyncedTabsProvider`](../../mozilla.components.feature.syncedtabs.storage/-synced-tabs-provider/index.md) |
| [view](view.md) | `abstract val view: `[`SyncedTabsView`](../../mozilla.components.feature.syncedtabs.view/-synced-tabs-view/index.md) |

### Functions

| Name | Summary |
|---|---|
| [refreshSyncedTabs](refresh-synced-tabs.md) | `abstract fun refreshSyncedTabs(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Requests for remote tabs and notifies the [SyncedTabsView](../../mozilla.components.feature.syncedtabs.view/-synced-tabs-view/index.md) when available with [SyncedTabsView.displaySyncedTabs](../../mozilla.components.feature.syncedtabs.view/-synced-tabs-view/display-synced-tabs.md) otherwise notifies the appropriate error to [SyncedTabsView.onError](../../mozilla.components.feature.syncedtabs.view/-synced-tabs-view/on-error.md). |
| [syncAccount](sync-account.md) | `abstract fun syncAccount(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Requests for the account on the [FxaAccountManager](../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md) to perform a sync. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
