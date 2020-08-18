/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.sync

import android.content.Context
import android.util.AttributeSet
import android.widget.FrameLayout
import androidx.fragment.app.findFragment
import androidx.navigation.NavController
import androidx.navigation.fragment.findNavController
import androidx.recyclerview.widget.LinearLayoutManager
import kotlinx.android.synthetic.main.component_sync_tabs.view.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import mozilla.components.browser.storage.sync.SyncedDeviceTabs
import mozilla.components.browser.storage.sync.Tab
import mozilla.components.feature.syncedtabs.view.SyncedTabsView
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.R
import org.mozilla.fenix.sync.ext.toAdapterItem
import org.mozilla.fenix.sync.ext.toStringRes
import java.lang.IllegalStateException

class SyncedTabsLayout @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : FrameLayout(context, attrs, defStyleAttr), SyncedTabsView {

    override var listener: SyncedTabsView.Listener? = null

    private val adapter = SyncedTabsAdapter(ListenerDelegate { listener })
    private val coroutineScope = CoroutineScope(Dispatchers.Main)

    init {
        inflate(getContext(), R.layout.component_sync_tabs, this)

        synced_tabs_list.layoutManager = LinearLayoutManager(context)
        synced_tabs_list.adapter = adapter

        synced_tabs_pull_to_refresh.setOnRefreshListener { listener?.onRefresh() }

        // Sanity-check: Remove this class when the feature flag is always enabled.
        FeatureFlags.syncedTabsInTabsTray
    }

    override fun onError(error: SyncedTabsView.ErrorType) {
        coroutineScope.launch {
            // We may still be displaying a "loading" spinner, hide it.
            stopLoading()

            val navController: NavController? = try {
                findFragment<SyncedTabsFragment>().findNavController()
            } catch (exception: IllegalStateException) {
                null
            }

            val descriptionResId = error.toStringRes()
            val errorItem = error.toAdapterItem(descriptionResId, navController)

            val errorList: List<SyncedTabsAdapter.AdapterItem> = listOf(errorItem)
            adapter.submitList(errorList)

            synced_tabs_pull_to_refresh.isEnabled = pullToRefreshEnableState(error)
        }
    }

    override fun displaySyncedTabs(syncedTabs: List<SyncedDeviceTabs>) {
        coroutineScope.launch {
            adapter.updateData(syncedTabs)
        }
    }

    override fun startLoading() {
        synced_tabs_pull_to_refresh.isRefreshing = true
    }

    override fun stopLoading() {
        synced_tabs_pull_to_refresh.isRefreshing = false
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()
        coroutineScope.cancel()
    }

    companion object {

        internal fun pullToRefreshEnableState(error: SyncedTabsView.ErrorType) = when (error) {
            // Disable "pull-to-refresh" when we clearly can't sync tabs, and user needs to take an
            // action within the app.
            SyncedTabsView.ErrorType.SYNC_UNAVAILABLE,
            SyncedTabsView.ErrorType.SYNC_NEEDS_REAUTHENTICATION -> false

            // Enable "pull-to-refresh" when an external event (e.g. connecting a desktop client,
            // or enabling tabs sync, or connecting to a network) may resolve our problem.
            SyncedTabsView.ErrorType.SYNC_ENGINE_UNAVAILABLE,
            SyncedTabsView.ErrorType.MULTIPLE_DEVICES_UNAVAILABLE,
            SyncedTabsView.ErrorType.NO_TABS_AVAILABLE -> true
        }
    }
}

/**
 * We have to do this weird daisy-chaining of callbacks because the listener is nullable and
 * when we get a null reference, we never get a new binding to the non-null listener.
 */
class ListenerDelegate(
    private val listener: (() -> SyncedTabsView.Listener?)
) : SyncedTabsView.Listener {
    override fun onRefresh() {
        listener.invoke()?.onRefresh()
    }

    override fun onTabClicked(tab: Tab) {
        listener.invoke()?.onTabClicked(tab)
    }
}
