/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.tabstray

import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListUpdateCallback
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.tabstray.Tabs
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.feature.tabs.ext.toTabs
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged

/**
 * Presenter implementation for a tabs tray implementation in order to update the tabs tray whenever
 * the state of the session manager changes.
 */
class TabsTrayPresenter(
    private val tabsTray: TabsTray,
    private val store: BrowserStore,
    internal var tabsFilter: (TabSessionState) -> Boolean,
    private val closeTabsTray: () -> Unit
) {
    private var tabs: Tabs? = null
    private var scope: CoroutineScope? = null

    fun start() {
        scope = store.flowScoped { flow -> collect(flow) }
    }

    fun stop() {
        scope?.cancel()
    }

    private suspend fun collect(flow: Flow<BrowserState>) {
        flow.map { it.toTabs(tabsFilter) }
            .ifChanged()
            .collect { tabs ->
                // Do not invoke the callback on start if this is the initial state.
                if (tabs.list.isEmpty() && this.tabs != null) {
                    closeTabsTray.invoke()
                }

                updateTabs(tabs)
            }
    }

    internal fun updateTabs(tabs: Tabs) {
        val currentTabs = this.tabs

        if (currentTabs == null) {
            this.tabs = tabs
            if (tabs.list.isNotEmpty()) {
                tabsTray.updateTabs(tabs)
                tabsTray.onTabsInserted(0, tabs.list.size)
            }
            return
        } else {
            calculateDiffAndUpdateTabsTray(currentTabs, tabs)
        }
    }

    /**
     * Calculates a diff between the last know state and the new state. After that it updates the
     * tab tray with the new data and notifies it about what changes happened so that it can animate
     * those changes.
     */
    private fun calculateDiffAndUpdateTabsTray(currentTabs: Tabs, updatedTabs: Tabs) {
        val result = DiffUtil.calculateDiff(DiffCallback(currentTabs, updatedTabs), true)

        this.tabs = updatedTabs

        tabsTray.updateTabs(updatedTabs)

        result.dispatchUpdatesTo(object : ListUpdateCallback {
            override fun onChanged(position: Int, count: Int, payload: Any?) {
                tabsTray.onTabsChanged(position, count)
            }

            override fun onMoved(fromPosition: Int, toPosition: Int) {
                tabsTray.onTabsMoved(fromPosition, toPosition)
            }

            override fun onInserted(position: Int, count: Int) {
                tabsTray.onTabsInserted(position, count)
            }

            override fun onRemoved(position: Int, count: Int) {
                tabsTray.onTabsRemoved(position, count)
            }
        })
    }
}

internal class DiffCallback(
    private val currentTabs: Tabs,
    private val updatedTabs: Tabs
) : DiffUtil.Callback() {
    override fun areItemsTheSame(oldItemPosition: Int, newItemPosition: Int): Boolean =
        currentTabs.list[oldItemPosition].id == updatedTabs.list[newItemPosition].id

    override fun getOldListSize(): Int = currentTabs.list.size

    override fun getNewListSize(): Int = updatedTabs.list.size

    override fun areContentsTheSame(oldItemPosition: Int, newItemPosition: Int): Boolean {
        if (oldItemPosition == currentTabs.selectedIndex && newItemPosition != updatedTabs.selectedIndex) {
            // This item was previously selected and is not selected anymore (-> changed).
            return false
        }

        if (newItemPosition == updatedTabs.selectedIndex && oldItemPosition != currentTabs.selectedIndex) {
            // This item was not selected previously and is now selected (-> changed).
            return false
        }

        return updatedTabs.list[newItemPosition] == currentTabs.list[oldItemPosition]
    }
}
