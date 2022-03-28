/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentsyncedtabs

import android.content.Context
import androidx.lifecycle.LifecycleOwner
import mozilla.components.browser.storage.sync.SyncedDeviceTabs
import mozilla.components.feature.syncedtabs.SyncedTabsFeature
import mozilla.components.feature.syncedtabs.storage.SyncedTabsStorage
import mozilla.components.feature.syncedtabs.view.SyncedTabsView
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction

/**
 * Delegate to handle layout updates and dispatch actions related to the recent synced tab.
 *
 * @property store Store to dispatch actions to when synced tabs are updated or errors encountered.
 * @param accountManager Account manager used to retrieve synced tab state.
 * @param context [Context] used for retrieving the sync engine storage state.
 * @param storage Storage layer for synced tabs.
 * @param lifecycleOwner View lifecycle owner to determine start/stop state for feature.
 */
@Suppress("LongParameterList")
class RecentSyncedTabFeature(
    private val store: AppStore,
    accountManager: FxaAccountManager,
    context: Context,
    storage: SyncedTabsStorage,
    lifecycleOwner: LifecycleOwner,
) : SyncedTabsView, LifecycleAwareFeature {
    private val syncedTabsFeature by lazy {
        SyncedTabsFeature(
            view = this,
            context = context,
            storage = storage,
            accountManager = accountManager,
            lifecycleOwner = lifecycleOwner,
            onTabClicked = {}
        )
    }

    override var listener: SyncedTabsView.Listener? = null

    override fun startLoading() {
        store.dispatch(
            AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.Loading)
        )
    }

    override fun displaySyncedTabs(syncedTabs: List<SyncedDeviceTabs>) {
        val syncedTab = syncedTabs
            .filterNot { it.device.isCurrentDevice || it.tabs.isEmpty() }
            .maxByOrNull { it.device.lastAccessTime ?: 0 }
            ?.let {
                val tab = it.tabs.firstOrNull()?.active() ?: return
                RecentSyncedTab(
                    deviceDisplayName = it.device.displayName,
                    title = tab.title,
                    url = tab.url,
                    iconUrl = tab.iconUrl
                )
            } ?: return
        store.dispatch(
            AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.Success(syncedTab))
        )
    }

    // UI will either not be displayed if not authenticated (DefaultPresenter.start),
    // or the display state will be tied directly to the success and error cases.
    override fun stopLoading() = Unit

    override fun onError(error: SyncedTabsView.ErrorType) {
        store.dispatch(AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.None))
    }

    override fun start() {
        syncedTabsFeature.start()
    }

    override fun stop() {
        syncedTabsFeature.stop()
    }
}

/**
 * The state of the recent synced tab.
 */
sealed class RecentSyncedTabState {
    /**
     * There is no synced tab, or a user is not authenticated.
     */
    object None : RecentSyncedTabState()

    /**
     * A user is authenticated and the sync is running.
     */
    object Loading : RecentSyncedTabState()

    /**
     * A user is authenticated and the most recent synced tab has been found.
     */
    data class Success(val tab: RecentSyncedTab) : RecentSyncedTabState()
}

/**
 * A tab that was recently viewed on a synced device.
 *
 * @param deviceDisplayName The device the tab was viewed on.
 * @param title The title of the tab.
 * @param url The url of the tab.
 * @param iconUrl The url used to retrieve the icon of the tab.
 */
data class RecentSyncedTab(
    val deviceDisplayName: String,
    val title: String,
    val url: String,
    val iconUrl: String?,
)
