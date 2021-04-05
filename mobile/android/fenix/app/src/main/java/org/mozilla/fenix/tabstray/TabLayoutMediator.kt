/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.annotation.VisibleForTesting
import com.google.android.material.tabs.TabLayout
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.fenix.tabstray.TrayPagerAdapter.Companion.POSITION_NORMAL_TABS
import org.mozilla.fenix.tabstray.TrayPagerAdapter.Companion.POSITION_PRIVATE_TABS

/**
 * Selected the selected pager depending on the [BrowserStore] state and synchronizes user actions
 * with the pager position.
 */
class TabLayoutMediator(
    private val tabLayout: TabLayout,
    private val interactor: TabsTrayInteractor,
    private val store: BrowserStore
) : LifecycleAwareFeature {

    private val observer = TabLayoutObserver(interactor)

    /**
     * Start observing the [TabLayout] and select the current tab for initial state.
     */
    override fun start() {
        tabLayout.addOnTabSelectedListener(observer)

        selectActivePage()
    }

    override fun stop() {
        tabLayout.removeOnTabSelectedListener(observer)
    }

    @VisibleForTesting
    internal fun selectActivePage() {
        val selectedTab = store.state.selectedTab ?: return

        val selectedPagerPosition = if (selectedTab.content.private) {
            POSITION_PRIVATE_TABS
        } else {
            POSITION_NORMAL_TABS
        }

        tabLayout.getTabAt(selectedPagerPosition)?.select()
    }
}

/**
 * An observer for the [TabLayout] used for the Tabs Tray.
 */
internal class TabLayoutObserver(
    private val interactor: TabsTrayInteractor
) : TabLayout.OnTabSelectedListener {
    override fun onTabSelected(tab: TabLayout.Tab) {
        interactor.setCurrentTrayPosition(tab.position)
    }

    override fun onTabUnselected(tab: TabLayout.Tab) = Unit
    override fun onTabReselected(tab: TabLayout.Tab) = Unit
}
