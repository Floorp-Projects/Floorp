[android-components](../../index.md) / [mozilla.components.feature.syncedtabs.presenter](../index.md) / [SyncedTabsPresenter](./index.md)

# SyncedTabsPresenter

`interface SyncedTabsPresenter : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/syncedtabs/src/main/java/mozilla/components/feature/syncedtabs/presenter/SyncedTabsPresenter.kt#L16)

A presenter that orchestrates the [FxaAccountManager](../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md) being in the correct state to request remote tabs from the
[SyncedTabsController](../../mozilla.components.feature.syncedtabs.controller/-synced-tabs-controller/index.md) or notifies [SyncedTabsView.onError](../../mozilla.components.feature.syncedtabs.view/-synced-tabs-view/on-error.md) otherwise.

### Properties

| Name | Summary |
|---|---|
| [accountManager](account-manager.md) | `abstract val accountManager: `[`FxaAccountManager`](../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md) |
| [controller](controller.md) | `abstract val controller: `[`SyncedTabsController`](../../mozilla.components.feature.syncedtabs.controller/-synced-tabs-controller/index.md) |
| [view](view.md) | `abstract val view: `[`SyncedTabsView`](../../mozilla.components.feature.syncedtabs.view/-synced-tabs-view/index.md) |

### Inherited Functions

| Name | Summary |
|---|---|
| [start](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/start.md) | `abstract fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [stop](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/stop.md) | `abstract fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
