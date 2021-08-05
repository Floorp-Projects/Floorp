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
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.concept.engine.Engine
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
import org.mozilla.fenix.utils.Settings

@OptIn(ExperimentalCoroutinesApi::class)
class DeleteAndQuitTest {

    val testDispatcher = TestCoroutineDispatcher()

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(testDispatcher)

    private val activity: HomeActivity = mockk(relaxed = true)
    private val settings: Settings = mockk(relaxed = true)
    private val tabUseCases: TabsUseCases = mockk(relaxed = true)
    private val historyStorage: PlacesHistoryStorage = mockk(relaxed = true)
    private val permissionStorage: PermissionStorage = mockk(relaxed = true)
    private val iconsStorage: BrowserIcons = mockk()
    private val engine: Engine = mockk(relaxed = true)
    private val removeAllTabsUseCases: TabsUseCases.RemoveAllTabsUseCase = mockk(relaxed = true)
    private val snackbar = mockk<FenixSnackbar>(relaxed = true)

    @Before
    fun setUp() {
        every { activity.components.core.historyStorage } returns historyStorage
        every { activity.components.core.permissionStorage } returns permissionStorage
        every { activity.components.useCases.tabsUseCases } returns tabUseCases
        every { tabUseCases.removeAllTabs } returns removeAllTabsUseCases
        every { activity.components.core.engine } returns engine
        every { activity.components.settings } returns settings
        every { activity.components.core.icons } returns iconsStorage
    }

    @After
    fun cleanUp() {
        testDispatcher.cleanupTestCoroutines()
    }

    @Test
    fun `delete only tabs and quit`() = runBlockingTest {
        // When
        every { settings.getDeleteDataOnQuit(DeleteBrowsingDataOnQuitType.TABS) } returns true

        deleteAndQuit(activity, this, snackbar)

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

    @Ignore("Intermittently failing; will be fixed with #5406.")
    @Test
    fun `delete everything and quit`() = runBlockingTest {
        // When
        DeleteBrowsingDataOnQuitType.values().forEach {
            every { settings.getDeleteDataOnQuit(it) } returns true
        }

        deleteAndQuit(activity, this, snackbar)

        coVerify(exactly = 1) {
            snackbar.show()

            engine.clearData(Engine.BrowsingData.allCaches())

            removeAllTabsUseCases.invoke()

            engine.clearData(
                Engine.BrowsingData.select(Engine.BrowsingData.ALL_SITE_SETTINGS)
            )

            permissionStorage.deleteAllSitePermissions()

            engine.clearData(
                Engine.BrowsingData.select(
                    Engine.BrowsingData.COOKIES,
                    Engine.BrowsingData.AUTH_SESSIONS
                )
            )

            engine.clearData(Engine.BrowsingData.select(Engine.BrowsingData.DOM_STORAGES))

            activity.finish()
        }

        coVerify {
            historyStorage.deleteEverything()
            iconsStorage.clear()
        }
    }
}
