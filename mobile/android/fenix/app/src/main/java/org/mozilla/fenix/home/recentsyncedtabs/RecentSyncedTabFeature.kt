/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentsyncedtabs

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.flow.onEach
import mozilla.components.concept.sync.DeviceType
import mozilla.components.feature.syncedtabs.storage.SyncedTabsStorage
import mozilla.components.lib.state.ext.flow
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.manager.ext.withConstellation
import mozilla.components.service.fxa.store.SyncStatus
import mozilla.components.service.fxa.store.SyncStore
import mozilla.components.service.fxa.sync.SyncReason
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import mozilla.telemetry.glean.GleanTimerId
import org.mozilla.fenix.GleanMetrics.RecentSyncedTabs

/**
 * Delegate to handle layout updates and dispatch actions related to the recent synced tab.
 *
 * @property appStore Store to dispatch actions to when synced tabs are updated or errors encountered.
 * @property syncStore Store to observe for changes to Sync and account status.
 * @property storage Storage layer for synced tabs.
 * @property accountManager Account manager to initiate Syncs and refresh devices.
 * @property coroutineScope The scope to collect Sync state Flow updates in.
 */
class RecentSyncedTabFeature(
    private val appStore: AppStore,
    private val syncStore: SyncStore,
    private val storage: SyncedTabsStorage,
    private val accountManager: FxaAccountManager,
    private val coroutineScope: CoroutineScope,
) : LifecycleAwareFeature {

    private var syncStartId: GleanTimerId? = null
    private var lastSyncedTab: RecentSyncedTab? = null

    override fun start() {
        collectAccountUpdates()
        collectStatusUpdates()
    }

    override fun stop() = Unit

    private fun collectAccountUpdates() {
        syncStore.flow()
            .ifChanged { state ->
                state.account != null
            }.onEach { state ->
                if (state.account != null) {
                    dispatchLoading()
                    // Sync tabs storage will fail to retrieve tabs aren't refreshed, as that action
                    // is what populates the device constellation state
                    accountManager.withConstellation { refreshDevices() }
                    accountManager.syncNow(SyncReason.User, customEngineSubset = listOf(SyncEngine.Tabs))
                }
            }.launchIn(coroutineScope)
    }

    private fun collectStatusUpdates() {
        syncStore.flow()
            .ifChanged { state ->
                state.status
            }.onEach { state ->
                when (state.status) {
                    SyncStatus.Idle -> dispatchSyncedTabs()
                    SyncStatus.Error -> onError()
                    SyncStatus.LoggedOut -> appStore.dispatch(
                        AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.None)
                    )
                    else -> Unit
                }
            }.launchIn(coroutineScope)
    }

    private fun dispatchLoading() {
        syncStartId?.let { RecentSyncedTabs.recentSyncedTabTimeToLoad.cancel(it) }
        syncStartId = RecentSyncedTabs.recentSyncedTabTimeToLoad.start()
        if (appStore.state.recentSyncedTabState == RecentSyncedTabState.None) {
            appStore.dispatch(AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.Loading))
        }
    }

    private suspend fun dispatchSyncedTabs() {
        val syncedTab = storage.getSyncedDeviceTabs()
            .filterNot { it.device.isCurrentDevice || it.tabs.isEmpty() }
            .maxByOrNull { it.device.lastAccessTime ?: 0 }
            ?.let {
                val tab = it.tabs.firstOrNull()?.active() ?: return
                RecentSyncedTab(
                    deviceDisplayName = it.device.displayName,
                    deviceType = it.device.deviceType,
                    title = tab.title,
                    url = tab.url,
                    iconUrl = tab.iconUrl
                )
            } ?: return
        recordMetrics(syncedTab, lastSyncedTab, syncStartId)
        appStore.dispatch(
            AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.Success(syncedTab))
        )
        lastSyncedTab = syncedTab
    }

    private fun onError() {
        if (appStore.state.recentSyncedTabState == RecentSyncedTabState.Loading) {
            appStore.dispatch(AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.None))
        }
    }

    private fun recordMetrics(
        tab: RecentSyncedTab,
        lastSyncedTab: RecentSyncedTab?,
        syncStartId: GleanTimerId?
    ) {
        RecentSyncedTabs.recentSyncedTabShown[tab.deviceType.name.lowercase()].add()
        syncStartId?.let { RecentSyncedTabs.recentSyncedTabTimeToLoad.stopAndAccumulate(it) }
        if (tab == lastSyncedTab) {
            RecentSyncedTabs.latestSyncedTabIsStale.add()
        }
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
    val deviceType: DeviceType,
    val title: String,
    val url: String,
    val iconUrl: String?,
)
