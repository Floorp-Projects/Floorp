/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.annotation.VisibleForTesting
import androidx.viewpager2.widget.ViewPager2
import com.google.android.material.tabs.TabLayout
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.tabstray.TrayPagerAdapter.Companion.POSITION_NORMAL_TABS
import org.mozilla.fenix.tabstray.TrayPagerAdapter.Companion.POSITION_PRIVATE_TABS
import org.mozilla.fenix.tabstray.TrayPagerAdapter.Companion.POSITION_SYNCED_TABS

/**
 * Selected the selected pager depending on the [BrowserStore] state and synchronizes user actions
 * with the pager position.
 */
class TabLayoutMediator(
    private val tabLayout: TabLayout,
    private val tabPager: ViewPager2,
    interactor: TabsTrayInteractor,
    private val browsingModeManager: BrowsingModeManager,
    private val tabsTrayStore: TabsTrayStore,
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
        val selectedPagerPosition =
            when {
                browsingModeManager.mode.isPrivate -> POSITION_PRIVATE_TABS
                tabsTrayStore.state.selectedPage == Page.SyncedTabs -> POSITION_SYNCED_TABS
                else -> POSITION_NORMAL_TABS
            }

        selectTabAtPosition(selectedPagerPosition)
    }

    fun selectTabAtPosition(position: Int) {
        tabLayout.getTabAt(position)?.select()
        tabPager.setCurrentItem(position, false)
        tabsTrayStore.dispatch(TabsTrayAction.PageSelected(Page.positionToPage(position)))
    }
}

/**
 * An observer for the [TabLayout] used for the Tabs Tray.
 */
internal class TabLayoutObserver(
    private val interactor: TabsTrayInteractor,
) : TabLayout.OnTabSelectedListener {

    private var initialScroll = true

    override fun onTabSelected(tab: TabLayout.Tab) {
        // Do not animate the initial scroll when opening the tabs tray.
        val animate = if (initialScroll) {
            initialScroll = false
            false
        } else {
            true
        }

        interactor.onTrayPositionSelected(tab.position, animate)
    }

    override fun onTabUnselected(tab: TabLayout.Tab) = Unit
    override fun onTabReselected(tab: TabLayout.Tab) = Unit
}
