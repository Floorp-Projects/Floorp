/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import androidx.navigation.NavController
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import io.mockk.verifyOrder
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
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
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class TabCounterViewTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    private lateinit var navController: NavController
    private lateinit var settings: Settings
    private lateinit var tabCounterView: TabCounterView
    private lateinit var tabCounter: TabCounter

    @Before
    fun setup() {
        navController = mockk(relaxed = true)
        settings = mockk(relaxed = true)

        tabCounter = spyk(TabCounter(testContext))
    }

    @Test
    fun `WHEN tab counter is clicked THEN navigate to tabs tray and record metrics`() {
        every { navController.currentDestination?.id } returns R.id.homeFragment
        assertNull(StartOnHome.openTabsTray.testGetValue())

        tabCounterView = createTabCounterView()
        tabCounter.performClick()

        assertNotNull(StartOnHome.openTabsTray.testGetValue())

        verify {
            navController.nav(
                R.id.homeFragment,
                NavGraphDirections.actionGlobalTabsTrayFragment(),
            )
        }
    }

    @Test
    fun `WHEN New tab menu item is tapped THEN set browsing mode to normal`() {
        var capture: BrowsingMode? = null
        tabCounterView = createTabCounterView {
            capture = it
        }

        tabCounterView.onItemTapped(TabCounterMenu.Item.NewTab)

        assertEquals(BrowsingMode.Normal, capture)
    }

    @Test
    fun `WHEN New private tab menu item is tapped THEN set browsing mode to private`() {
        var capture: BrowsingMode? = null
        tabCounterView = createTabCounterView {
            capture = it
        }

        tabCounterView.onItemTapped(TabCounterMenu.Item.NewPrivateTab)

        assertEquals(BrowsingMode.Private, capture)
    }

    @Test
    fun `WHEN tab counter is updated THEN set the tab counter to the correct number of tabs`() {
        every { testContext.settings() } returns settings

        val browserState = BrowserState(
            tabs = listOf(
                createTab(url = "https://www.mozilla.org", id = "mozilla"),
                createTab(url = "https://www.firefox.com", id = "firefox"),
                createTab(url = "https://getpocket.com", private = true, id = "getpocket"),
            ),
            selectedTabId = "mozilla",
        )
        tabCounterView = createTabCounterView()

        tabCounterView.update(browserState)

        verify {
            tabCounter.setCountWithAnimation(browserState.normalTabs.size)
        }

        tabCounterView = createTabCounterView(mode = BrowsingMode.Private)

        tabCounterView.update(browserState)

        verify {
            tabCounter.setCountWithAnimation(browserState.privateTabs.size)
        }
    }

    @Test
    fun `WHEN state updated while in private mode THEN call toggleCounterMask(true)`() {
        every { settings.feltPrivateBrowsingEnabled } returns true
        every { testContext.settings() } returns settings
        val browserState = BrowserState(
            tabs = listOf(
                createTab(url = "https://www.mozilla.org", id = "mozilla"),
                createTab(url = "https://www.firefox.com", id = "firefox"),
                createTab(url = "https://getpocket.com", private = true, id = "getpocket"),
            ),
            selectedTabId = "mozilla",
        )

        tabCounterView = createTabCounterView(mode = BrowsingMode.Private)
        tabCounterView.update(browserState)

        verify {
            tabCounter.toggleCounterMask(true)
        }
    }

    @Test
    fun `WHEN state updated while in normal mode THEN call toggleCounterMask(false)`() {
        every { settings.feltPrivateBrowsingEnabled } returns true
        every { testContext.settings() } returns settings
        val browserState = BrowserState(
            tabs = listOf(
                createTab(url = "https://www.mozilla.org", id = "mozilla"),
                createTab(url = "https://www.firefox.com", id = "firefox"),
                createTab(url = "https://getpocket.com", private = true, id = "getpocket"),
            ),
            selectedTabId = "mozilla",
        )

        tabCounterView = createTabCounterView()
        tabCounterView.update(browserState)

        verifyOrder {
            tabCounter.toggleCounterMask(false)
        }
    }

    private fun createTabCounterView(
        mode: BrowsingMode = BrowsingMode.Normal,
        itemTapped: (BrowsingMode) -> Unit = {},
    ) = TabCounterView(
        context = testContext,
        navController = navController,
        tabCounter = tabCounter,
        mode = mode,
        itemTapped = itemTapped,
    )
}
