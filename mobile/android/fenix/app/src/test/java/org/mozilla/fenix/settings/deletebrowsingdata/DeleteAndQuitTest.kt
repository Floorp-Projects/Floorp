/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("DEPRECATION")

package org.mozilla.fenix.settings.deletebrowsingdata

import io.mockk.coVerify
import io.mockk.every
import io.mockk.mockk
import io.mockk.verifyOrder
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.downloads.DownloadsUseCases.RemoveAllDownloadsUseCase
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.PermissionStorage
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.settings.deletebrowsingdata.DeleteBrowsingDataOnQuitType.TABS
import org.mozilla.fenix.settings.deletebrowsingdata.DeleteBrowsingDataOnQuitType.CACHE
import org.mozilla.fenix.settings.deletebrowsingdata.DeleteBrowsingDataOnQuitType.COOKIES
import org.mozilla.fenix.settings.deletebrowsingdata.DeleteBrowsingDataOnQuitType.DOWNLOADS
import org.mozilla.fenix.settings.deletebrowsingdata.DeleteBrowsingDataOnQuitType.PERMISSIONS
import org.mozilla.fenix.settings.deletebrowsingdata.DeleteBrowsingDataOnQuitType.HISTORY
import org.mozilla.fenix.utils.Settings

@OptIn(ExperimentalCoroutinesApi::class)
class DeleteAndQuitTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private val activity: HomeActivity = mockk(relaxed = true)
    private val settings: Settings = mockk(relaxed = true)
    private val tabUseCases: TabsUseCases = mockk(relaxed = true)
    private val historyStorage: PlacesHistoryStorage = mockk(relaxed = true)
    private val permissionStorage: PermissionStorage = mockk(relaxed = true)
    private val iconsStorage: BrowserIcons = mockk()
    private val engine: Engine = mockk(relaxed = true)
    private val removeAllTabsUseCases: TabsUseCases.RemoveAllTabsUseCase = mockk(relaxed = true)
    private val snackbar = mockk<FenixSnackbar>(relaxed = true)
    private val downloadsUseCases: RemoveAllDownloadsUseCase = mockk(relaxed = true)

    @Before
    fun setUp() {
        every { activity.components.core.historyStorage } returns historyStorage
        every { activity.components.core.permissionStorage } returns permissionStorage
        every { activity.components.useCases.tabsUseCases } returns tabUseCases
        every { activity.components.useCases.downloadUseCases.removeAllDownloads } returns downloadsUseCases
        every { tabUseCases.removeAllTabs } returns removeAllTabsUseCases
        every { activity.components.core.engine } returns engine
        every { activity.components.settings } returns settings
        every { activity.components.core.icons } returns iconsStorage
    }

    @After
    fun cleanUp() {
        coroutinesTestRule.testDispatcher.cleanupTestCoroutines()
    }

    @Ignore("Failing test; need more investigation.")
    @Test
    fun `delete only tabs and quit`() = runBlockingTest {
        // When
        every { settings.getDeleteDataOnQuit(TABS) } returns true

        deleteAndQuit(activity, this, snackbar)

        coroutinesTestRule.testDispatcher.advanceUntilIdle()

        verifyOrder {
            snackbar.show()
            removeAllTabsUseCases.invoke(false)
            activity.finishAndRemoveTask()
        }

        coVerify(exactly = 0) {
            engine.clearData(
                Engine.BrowsingData.select(
                    Engine.BrowsingData.COOKIES
                )
            )

            permissionStorage.deleteAllSitePermissions()

            engine.clearData(Engine.BrowsingData.allCaches())
        }

        coVerify(exactly = 0) {
            historyStorage.deleteEverything()
            iconsStorage.clear()
        }
    }

    @Ignore("Failing test; need more investigation.")
    @Test
    fun `delete everything and quit`() = runBlockingTest {
        // When
        every { settings.getDeleteDataOnQuit(TABS) } returns true
        every { settings.getDeleteDataOnQuit(HISTORY) } returns true
        every { settings.getDeleteDataOnQuit(COOKIES) } returns true
        every { settings.getDeleteDataOnQuit(CACHE) } returns true
        every { settings.getDeleteDataOnQuit(PERMISSIONS) } returns true
        every { settings.getDeleteDataOnQuit(DOWNLOADS) } returns true

        deleteAndQuit(activity, this, snackbar)

        coroutinesTestRule.testDispatcher.advanceUntilIdle()

        coVerify(exactly = 1) {
            snackbar.show()

            // Delete tabs
            removeAllTabsUseCases.invoke(false)

            // Delete browsing data
            engine.clearData(Engine.BrowsingData.select(Engine.BrowsingData.DOM_STORAGES))
            historyStorage.deleteEverything()
            iconsStorage.clear()

            // Delete cookies
            engine.clearData(
                Engine.BrowsingData.select(
                    Engine.BrowsingData.COOKIES,
                    Engine.BrowsingData.AUTH_SESSIONS
                )
            )

            // Delete cached files
            engine.clearData(Engine.BrowsingData.select(Engine.BrowsingData.ALL_CACHES))

            // Delete permissions
            engine.clearData(Engine.BrowsingData.select(Engine.BrowsingData.ALL_SITE_SETTINGS))
            permissionStorage.deleteAllSitePermissions()

            // Delete downloads
            downloadsUseCases.invoke()

            // Finish activity
            activity.finishAndRemoveTask()
        }
    }
}
