/*  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.deletebrowsingdata

import io.mockk.coVerify
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.GlobalScope.coroutineContext
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.components.PermissionStorage

@OptIn(ExperimentalCoroutinesApi::class)
class DefaultDeleteBrowsingDataControllerTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(TestCoroutineDispatcher())

    private var removeAllTabs: TabsUseCases.RemoveAllTabsUseCase = mockk(relaxed = true)
    private var historyStorage: HistoryStorage = mockk(relaxed = true)
    private var permissionStorage: PermissionStorage = mockk(relaxed = true)
    private var iconsStorage: BrowserIcons = mockk(relaxed = true)
    private val engine: Engine = mockk(relaxed = true)
    private lateinit var controller: DefaultDeleteBrowsingDataController

    @Before
    fun setup() {
        controller = DefaultDeleteBrowsingDataController(
            removeAllTabs = removeAllTabs,
            historyStorage = historyStorage,
            permissionStorage = permissionStorage,
            iconsStorage = iconsStorage,
            engine = engine,
            coroutineContext = coroutineContext
        )
    }

    @Test
    fun deleteTabs() = runBlockingTest {

        controller.deleteTabs()

        verify {
            removeAllTabs.invoke()
        }
    }

    @Test
    fun deleteBrowsingData() = runBlockingTest {
        controller = spyk(controller)
        controller.deleteBrowsingData()

        coVerify {
            engine.clearData(Engine.BrowsingData.select(Engine.BrowsingData.DOM_STORAGES))
            historyStorage.deleteEverything()
            iconsStorage.clear()
        }
    }

    @Test
    fun deleteCookies() = runBlockingTest {
        controller.deleteCookies()

        verify {
            engine.clearData(
                Engine.BrowsingData.select(
                    Engine.BrowsingData.COOKIES,
                    Engine.BrowsingData.AUTH_SESSIONS
                )
            )
        }
    }

    @Test
    fun deleteCachedFiles() = runBlockingTest {

        controller.deleteCachedFiles()

        verify {
            engine.clearData(Engine.BrowsingData.select(Engine.BrowsingData.ALL_CACHES))
        }
    }

    @Test
    fun deleteSitePermissions() = runBlockingTest {
        controller.deleteSitePermissions()

        verify {
            engine.clearData(Engine.BrowsingData.select(Engine.BrowsingData.ALL_SITE_SETTINGS))
            permissionStorage.deleteAllSitePermissions()
        }
    }
}
