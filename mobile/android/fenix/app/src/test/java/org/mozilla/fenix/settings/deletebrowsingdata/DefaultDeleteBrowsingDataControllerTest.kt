/*  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.deletebrowsingdata

import android.content.Context
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.concept.engine.Engine
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.ext.components

@OptIn(ExperimentalCoroutinesApi::class)
class DefaultDeleteBrowsingDataControllerTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(TestCoroutineDispatcher())

    private val context: Context = mockk(relaxed = true)
    private lateinit var controller: DefaultDeleteBrowsingDataController

    @Before
    fun setup() {
        every { context.components.core.engine.clearData(any()) } just Runs
    }

    @Test
    fun deleteTabs() = runBlockingTest {
        controller = DefaultDeleteBrowsingDataController(context, coroutineContext)
        every { context.components.useCases.tabsUseCases.removeAllTabs.invoke() } just Runs

        controller.deleteTabs()

        verify {
            context.components.useCases.tabsUseCases.removeAllTabs.invoke()
        }
    }

    @Test
    fun deleteBrowsingData() = runBlockingTest {
        controller = DefaultDeleteBrowsingDataController(context, coroutineContext)
        every { context.components.core.historyStorage } returns mockk(relaxed = true)

        controller.deleteBrowsingData()

        verify {
            context.components.core.engine.clearData(any())
            context.components.core.historyStorage
        }
    }

    @Test
    fun deleteCookies() = runBlockingTest {
        controller = DefaultDeleteBrowsingDataController(context, coroutineContext)

        controller.deleteCookies()

        verify {
            context.components.core.engine.clearData(
                Engine.BrowsingData.select(
                    Engine.BrowsingData.COOKIES,
                    Engine.BrowsingData.AUTH_SESSIONS
                )
            )
        }
    }

    @Test
    fun deleteCachedFiles() = runBlockingTest {
        controller = DefaultDeleteBrowsingDataController(context, coroutineContext)

        controller.deleteCachedFiles()

        verify {
            context.components.core.engine.clearData(Engine.BrowsingData.select(Engine.BrowsingData.ALL_CACHES))
        }
    }

    @Test
    fun deleteSitePermissions() = runBlockingTest {
        controller = DefaultDeleteBrowsingDataController(context, coroutineContext)
        every { context.components.core.permissionStorage.deleteAllSitePermissions() } just Runs

        controller.deleteSitePermissions()

        verify {
            context.components.core.engine.clearData(Engine.BrowsingData.select(Engine.BrowsingData.ALL_SITE_SETTINGS))
            context.components.core.permissionStorage.deleteAllSitePermissions()
        }
    }
}
