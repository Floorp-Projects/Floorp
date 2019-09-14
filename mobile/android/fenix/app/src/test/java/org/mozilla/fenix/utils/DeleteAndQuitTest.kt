/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("DEPRECATION")

package org.mozilla.fenix.utils

import androidx.test.ext.junit.runners.AndroidJUnit4
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
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
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.TestApplication
import org.mozilla.fenix.components.PermissionStorage
import org.mozilla.fenix.ext.clearAndCommit
import org.mozilla.fenix.ext.components
import org.robolectric.annotation.Config

@ObsoleteCoroutinesApi
@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
@Config(application = TestApplication::class)
class DeleteAndQuitTest {

    private val mainThreadSurrogate = newSingleThreadContext("UI thread")

    private var activity: HomeActivity = mockk(relaxed = true)
    lateinit var settings: Settings
    private val tabUseCases: TabsUseCases = mockk(relaxed = true)
    private val historyStorage: PlacesHistoryStorage = mockk(relaxed = true)
    private val permissionStorage: PermissionStorage = mockk(relaxed = true)
    private val engine: Engine = mockk(relaxed = true)
    private val removeAllTabsUseCases: TabsUseCases.RemoveAllTabsUseCase = mockk(relaxed = true)

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
        settings.deleteTabsOnQuit = true

        deleteAndQuit(activity, this)

        verify {
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

    @Test
    fun `delete everything and quit`() = runBlockingTest {
        // When
        settings.deleteTabsOnQuit = true
        settings.deletePermissionsOnQuit = true
        settings.deleteHistoryOnQuit = true
        settings.deleteCookiesOnQuit = true
        settings.deleteCacheOnQuit = true

        deleteAndQuit(activity, this)

        verify(exactly = 1) {
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
