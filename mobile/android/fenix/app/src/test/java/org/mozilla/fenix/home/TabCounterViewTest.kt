/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import androidx.navigation.NavController
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.ui.tabcounter.TabCounter
import mozilla.components.ui.tabcounter.TabCounterMenu
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.StartOnHome
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.browser.browsingmode.DefaultBrowsingModeManager
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class TabCounterViewTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    private lateinit var navController: NavController
    private lateinit var browsingModeManager: BrowsingModeManager
    private lateinit var settings: Settings
    private lateinit var modeDidChange: (BrowsingMode) -> Unit
    private lateinit var tabCounterView: TabCounterView
    private lateinit var tabCounter: TabCounter

    @Before
    fun setup() {
        navController = mockk(relaxed = true)
        settings = mockk(relaxed = true)
        modeDidChange = mockk(relaxed = true)

        tabCounter = TabCounter(testContext)

        browsingModeManager = DefaultBrowsingModeManager(
            _mode = BrowsingMode.Normal,
            settings = settings,
            modeDidChange = modeDidChange,
        )

        tabCounterView = TabCounterView(
            context = testContext,
            browsingModeManager = browsingModeManager,
            navController = navController,
            tabCounter = tabCounter,
        )
    }

    @Test
    fun `WHEN tab counter is clicked THEN navigate to tabs tray and record metrics`() {
        assertNull(StartOnHome.openTabsTray.testGetValue())

        tabCounter.performClick()

        assertNotNull(StartOnHome.openTabsTray.testGetValue())

        verify {
            navController.nav(
                R.id.homeFragment,
                HomeFragmentDirections.actionGlobalTabsTrayFragment(),
            )
        }
    }

    @Test
    fun `WHEN New tab menu item is tapped THEN set browsing mode to normal`() {
        tabCounterView.onItemTapped(TabCounterMenu.Item.NewTab)

        assertEquals(BrowsingMode.Normal, browsingModeManager.mode)
    }

    @Test
    fun `WHEN New private tab menu item is tapped THEN set browsing mode to private`() {
        tabCounterView.onItemTapped(TabCounterMenu.Item.NewPrivateTab)

        assertEquals(BrowsingMode.Private, browsingModeManager.mode)
    }
}
