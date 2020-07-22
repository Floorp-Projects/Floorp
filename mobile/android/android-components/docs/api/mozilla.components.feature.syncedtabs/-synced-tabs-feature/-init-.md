[android-components](../../index.md) / [mozilla.components.feature.syncedtabs](../index.md) / [SyncedTabsFeature](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`SyncedTabsFeature(context: <ERROR CLASS>, storage: `[`SyncedTabsStorage`](../../mozilla.components.feature.syncedtabs.storage/-synced-tabs-storage/index.md)`, accountManager: `[`FxaAccountManager`](../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md)`, view: `[`SyncedTabsView`](../../mozilla.components.feature.syncedtabs.view/-synced-tabs-view/index.md)`, lifecycleOwner: LifecycleOwner, coroutineContext: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)` = Dispatchers.IO, onTabClicked: (`[`Tab`](../../mozilla.components.browser.storage.sync/-tab/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, controller: `[`SyncedTabsController`](../../mozilla.components.feature.syncedtabs.controller/-synced-tabs-controller/index.md)` = DefaultController(
        storage,
        accountManager,
        view,
        coroutineContext
    ), presenter: `[`SyncedTabsPresenter`](../../mozilla.components.feature.syncedtabs.presenter/-synced-tabs-presenter/index.md)` = DefaultPresenter(
        context,
        controller,
        accountManager,
        view,
        lifecycleOwner
    ), interactor: `[`SyncedTabsInteractor`](../../mozilla.components.feature.syncedtabs.interactor/-synced-tabs-interactor/index.md)` = DefaultInteractor(
        controller,
        view,
        onTabClicked
    ))`

Feature implementation that will keep a [SyncedTabsView](../../mozilla.components.feature.syncedtabs.view/-synced-tabs-view/index.md) notified with other synced device tabs for
the Firefox Sync account.

### Parameters

`storage` - The synced tabs storage that stores the current device's and remote device tabs.

`accountManager` - Firefox Account Manager that holds a Firefox Sync account.

`view` - An implementor of [SyncedTabsView](../../mozilla.components.feature.syncedtabs.view/-synced-tabs-view/index.md) that will be notified of changes.

`lifecycleOwner` - Android Lifecycle Owner to bind observers onto.

`coroutineContext` - A coroutine context that can be used to perform work off the main thread.

`onTabClicked` - Invoked when a tab is selected by the user on the [SyncedTabsView](../../mozilla.components.feature.syncedtabs.view/-synced-tabs-view/index.md).

`controller` - See [SyncedTabsController](../../mozilla.components.feature.syncedtabs.controller/-synced-tabs-controller/index.md).

`presenter` - See [SyncedTabsPresenter](../../mozilla.components.feature.syncedtabs.presenter/-synced-tabs-presenter/index.md).

`interactor` - See [SyncedTabsInteractor](../../mozilla.components.feature.syncedtabs.interactor/-synced-tabs-interactor/index.md).