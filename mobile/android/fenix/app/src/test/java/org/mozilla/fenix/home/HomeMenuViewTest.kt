/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.view.View
import androidx.lifecycle.LifecycleOwner
import androidx.navigation.NavController
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.menu.view.MenuButton
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.HomeScreen
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.accounts.AccountState
import org.mozilla.fenix.components.accounts.FenixFxAEntryPoint
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.whatsnew.WhatsNew
import java.lang.ref.WeakReference
import org.mozilla.fenix.GleanMetrics.HomeMenu as HomeMenuMetrics

@RunWith(FenixRobolectricTestRunner::class)
class HomeMenuViewTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    private lateinit var view: View
    private lateinit var lifecycleOwner: LifecycleOwner
    private lateinit var homeActivity: HomeActivity
    private lateinit var navController: NavController
    private lateinit var menuButton: MenuButton
    private lateinit var homeMenuView: HomeMenuView

    @Before
    fun setup() {
        view = mockk(relaxed = true)
        lifecycleOwner = mockk(relaxed = true)
        homeActivity = mockk(relaxed = true)
        navController = mockk(relaxed = true)

        menuButton = spyk(MenuButton(testContext))

        homeMenuView = HomeMenuView(
            view = view,
            context = testContext,
            lifecycleOwner = lifecycleOwner,
            homeActivity = homeActivity,
            navController = navController,
            menuButton = WeakReference(menuButton),
        )
    }

    @Test
    fun `WHEN dismiss menu is called THEN the menu is dismissed`() {
        homeMenuView.dismissMenu()

        verify {
            menuButton.dismissMenu()
        }
    }

    @Test
    fun `WHEN Settings menu item is tapped THEN navigate to settings fragment and record metrics`() {
        assertNull(HomeMenuMetrics.settingsItemClicked.testGetValue())

        homeMenuView.onItemTapped(HomeMenu.Item.Settings)

        assertNotNull(HomeMenuMetrics.settingsItemClicked.testGetValue())

        verify {
            navController.nav(
                R.id.homeFragment,
                HomeFragmentDirections.actionGlobalSettingsFragment(),
            )
        }
    }

    @Test
    fun `WHEN Customize Home menu item is tapped THEN navigate to home settings fragment and record metrics`() {
        assertNull(HomeScreen.customizeHomeClicked.testGetValue())

        homeMenuView.onItemTapped(HomeMenu.Item.CustomizeHome)

        assertNotNull(HomeScreen.customizeHomeClicked.testGetValue())

        verify {
            navController.nav(
                R.id.homeFragment,
                HomeFragmentDirections.actionGlobalHomeSettingsFragment(),
            )
        }
    }

    @Test
    fun `GIVEN various sync account state WHEN Sync Account menu item is tapped THEN navigate to the appropriate sync fragment`() {
        homeMenuView.onItemTapped(HomeMenu.Item.SyncAccount(AccountState.AUTHENTICATED))

        verify {
            navController.nav(
                R.id.homeFragment,
                HomeFragmentDirections.actionGlobalAccountSettingsFragment(),
            )
        }

        homeMenuView.onItemTapped(HomeMenu.Item.SyncAccount(AccountState.NEEDS_REAUTHENTICATION))

        verify {
            navController.nav(
                R.id.homeFragment,
                HomeFragmentDirections.actionGlobalAccountProblemFragment(
                    entrypoint = FenixFxAEntryPoint.HomeMenu,
                ),
            )
        }

        homeMenuView.onItemTapped(HomeMenu.Item.SyncAccount(AccountState.NO_ACCOUNT))

        verify {
            navController.nav(
                R.id.homeFragment,
                HomeFragmentDirections.actionGlobalTurnOnSync(entrypoint = FenixFxAEntryPoint.HomeMenu),
            )
        }
    }

    @Test
    fun `WHEN Bookmarks menu item is tapped THEN navigate to the bookmarks fragment`() {
        homeMenuView.onItemTapped(HomeMenu.Item.Bookmarks)

        verify {
            navController.nav(
                R.id.homeFragment,
                HomeFragmentDirections.actionGlobalBookmarkFragment(BookmarkRoot.Mobile.id),
            )
        }
    }

    @Test
    fun `WHEN History menu item is tapped THEN navigate to the history fragment`() {
        homeMenuView.onItemTapped(HomeMenu.Item.History)

        verify {
            navController.nav(
                R.id.homeFragment,
                HomeFragmentDirections.actionGlobalHistoryFragment(),
            )
        }
    }

    @Test
    fun `WHEN Downloads menu item is tapped THEN navigate to the downloads fragment`() {
        homeMenuView.onItemTapped(HomeMenu.Item.Downloads)

        verify {
            navController.nav(
                R.id.homeFragment,
                HomeFragmentDirections.actionGlobalDownloadsFragment(),
            )
        }
    }

    @Test
    fun `WHEN Help menu item is tapped THEN open the browser to the SUMO help page`() {
        homeMenuView.onItemTapped(HomeMenu.Item.Help)

        verify {
            homeActivity.openToBrowserAndLoad(
                searchTermOrURL = SupportUtils.getSumoURLForTopic(
                    context = testContext,
                    topic = SupportUtils.SumoTopic.HELP,
                ),
                newTab = true,
                from = BrowserDirection.FromHome,
            )
        }
    }

    @Test
    fun `WHEN Whats New menu item is tapped THEN open the browser to the SUMO whats new page and record metrics`() {
        assertNull(Events.whatsNewTapped.testGetValue())

        homeMenuView.onItemTapped(HomeMenu.Item.WhatsNew)

        assertNotNull(Events.whatsNewTapped.testGetValue())

        verify {
            WhatsNew.userViewedWhatsNew(testContext)

            homeActivity.openToBrowserAndLoad(
                searchTermOrURL = SupportUtils.WHATS_NEW_URL,
                newTab = true,
                from = BrowserDirection.FromHome,
            )
        }
    }

    @Test
    fun `WHEN Reconnect Sync menu item is tapped THEN navigate to the account problem fragment`() {
        homeMenuView.onItemTapped(HomeMenu.Item.ReconnectSync)

        verify {
            navController.nav(
                R.id.homeFragment,
                HomeFragmentDirections.actionGlobalAccountProblemFragment(
                    entrypoint = FenixFxAEntryPoint.HomeMenu,
                ),
            )
        }
    }

    @Test
    fun `WHEN Extensions menu item is tapped THEN navigate to the addons management fragment`() {
        homeMenuView.onItemTapped(HomeMenu.Item.Extensions)

        verify {
            navController.nav(
                R.id.homeFragment,
                HomeFragmentDirections.actionGlobalAddonsManagementFragment(),
            )
        }
    }

    @Test
    fun `WHEN Desktop Mode menu item is tapped THEN set the desktop mode settings`() {
        every { testContext.settings() } returns Settings(testContext)

        homeMenuView.onItemTapped(HomeMenu.Item.DesktopMode(checked = true))

        assertTrue(testContext.settings().openNextTabInDesktopMode)
    }
}
