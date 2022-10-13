/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.tabstray

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.state.state.TabPartition
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.tabstray.TabsTray
import mozilla.components.feature.tabs.ext.toTabList
import mozilla.components.support.base.feature.LifecycleAwareFeature

/**
 * Feature implementation for connecting a tabs tray implementation with the session module.
 *
 * @param defaultTabsFilter A tab filter that is used for the initial presenting of tabs.
 * @param defaultTabPartitionsFilter A tab partition filter that is used for the initial presenting of
 * tabs.
 * @param onCloseTray a callback invoked when the last tab is closed.
 */
@Suppress("LongParameterList")
class TabsFeature(
    private val tabsTray: TabsTray,
    private val store: BrowserStore,
    private val onCloseTray: () -> Unit = {},
    private val defaultTabPartitionsFilter: (Map<String, TabPartition>) -> TabPartition? = { null },
    private val defaultTabsFilter: (TabSessionState) -> Boolean = { true },
) : LifecycleAwareFeature {
    @VisibleForTesting
    internal var presenter = TabsTrayPresenter(
        tabsTray,
        store,
        defaultTabsFilter,
        defaultTabPartitionsFilter,
        onCloseTray,
    )

    override fun start() {
        presenter.start()
    }

    override fun stop() {
        presenter.stop()
    }

    /**
     * Filter the list of tabs using [tabsFilter].
     *
     * @param tabsFilter A filter function returning `true` for all tabs that should be displayed in
     * the tabs tray. Uses the [defaultTabsFilter] if none is provided.
     */
    fun filterTabs(tabsFilter: (TabSessionState) -> Boolean = defaultTabsFilter) {
        presenter.tabsFilter = tabsFilter

        val state = store.state
        val (tabs, selectedTabId) = state.toTabList(tabsFilter)

        tabsTray.updateTabs(tabs, null, selectedTabId)
    }
}
