/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("DEPRECATION")

package org.mozilla.fenix.settings.deletebrowsingdata

import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import io.mockk.verifyOrder
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.newSingleThreadContext
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.runBlockingTest
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.PermissionStorage
import org.mozilla.fenix.ext.clearAndCommit
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.utils.Settings

@ExperimentalCoroutinesApi
@RunWith(FenixRobolectricTestRunner::class)
class DeleteAndQuitTest {

    private val mainThreadSurrogate = newSingleThreadContext("UI thread")

    private var activity: HomeActivity = mockk(relaxed = true)
    lateinit var settings: Settings
    private val tabUseCases: TabsUseCases = mockk(relaxed = true)
    private val historyStorage: PlacesHistoryStorage = mockk(relaxed = true)
    private val permissionStorage: PermissionStorage = mockk(relaxed = true)
    private val engine: Engine = mockk(relaxed = true)
    private val removeAllTabsUseCases: TabsUseCases.RemoveAllTabsUseCase = mockk(relaxed = true)
    private val snackbar = mockk<FenixSnackbar>(relaxed = true)

    @Before
    fun setUp() {
        settings = Settings.getInstance(testContext).apply {
            clear()
        }

        Dispatchers.setMain(mainThreadSurrogate)

        every { activity.components.core.historyStorage } returns historyStorage
        every { activity.components.core.permissionStorage } returns permissionStorage
        every { activity.components.useCases.tabsUseCases } returns tabUseCases
        every { tabUseCases.removeAllTabs } returns removeAllTabsUseCases
        every { activity.components.core.engine } returns engine
    }

    @After
    fun tearDown() {
        Dispatchers.resetMain() // reset main dispatcher to the original Main dispatcher
        mainThreadSurrogate.close()
    }

    private fun Settings.clear() {
        preferences.clearAndCommit()
    }

    @Test
    fun `delete only tabs and quit`() = runBlockingTest {
        // When
        settings.setDeleteDataOnQuit(DeleteBrowsingDataOnQuitType.TABS, true)

        deleteAndQuit(activity, this, snackbar)

        verifyOrder {
            snackbar.show()
            removeAllTabsUseCases.invoke()
            activity.finish()
        }

        verify(exactly = 0) {
            historyStorage

            engine.clearData(
                Engine.BrowsingData.select(
                    Engine.BrowsingData.COOKIES
                )
            )

            permissionStorage.deleteAllSitePermissions()

            engine.clearData(Engine.BrowsingData.allCaches())
        }
    }

    @Ignore("Intermittently failing; will be fixed with #5406.")
    @Test
    fun `delete everything and quit`() = runBlockingTest {
        // When
        DeleteBrowsingDataOnQuitType.values().forEach {
            settings.setDeleteDataOnQuit(it, true)
        }

        deleteAndQuit(activity, this, snackbar)

        verify(exactly = 1) {
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

            historyStorage

            activity.finish()
        }
    }
}
