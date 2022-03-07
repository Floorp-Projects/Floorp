/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recenttabs

import android.graphics.Bitmap
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.helpers.AbstractBinding
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.ext.asRecentTabs

/**
 * View-bound feature that dispatches recent tab changes to the [AppStore] when the
 * [BrowserStore] is updated.
 */
@OptIn(ExperimentalCoroutinesApi::class)
class RecentTabsListFeature(
    browserStore: BrowserStore,
    private val appStore: AppStore
) : AbstractBinding<BrowserState>(browserStore) {

    override suspend fun onState(flow: Flow<BrowserState>) {
        // Listen for changes regarding the currently selected tab, in progress media tab
        // and search term groups.
        flow
            .map { it.asRecentTabs() }
            .ifChanged()
            .collect {
                appStore.dispatch(AppAction.RecentTabsChange(it))
            }
    }
}

sealed class RecentTab {
    /**
     * A tab that was recently viewed
     *
     * @param state Recently viewed [TabSessionState]
     */
    data class Tab(val state: TabSessionState) : RecentTab()

    /**
     * A tab that was recently viewed on a synced device.
     *
     * @param deviceDisplayName The device the tab was viewed on.
     * @param title The title of the tab.
     * @param url The url of the tab.
     * @param previewImageUrl The url used to retrieve the preview image of the tab.
     */
    data class SyncedTab(
        val deviceDisplayName: String,
        val title: String,
        val url: String,
        val previewImageUrl: String?,
    ) : RecentTab()

    /**
     * A search term group that was recently viewed
     *
     * @param searchTerm The search term that was recently viewed. Forced to start with uppercase.
     * @param tabId The id of the tab that was recently viewed
     * @param url The url that was recently viewed
     * @param thumbnail The thumbnail of the search term that was recently viewed
     * @param count The number of tabs in the search term group
     */
    data class SearchGroup(
        val searchTerm: String,
        val tabId: String,
        val url: String,
        val thumbnail: Bitmap?,
        val count: Int
    ) : RecentTab()
}
