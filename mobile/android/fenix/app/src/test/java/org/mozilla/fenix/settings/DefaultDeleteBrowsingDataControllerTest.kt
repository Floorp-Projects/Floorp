/*  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import kotlinx.coroutines.launch
import kotlinx.coroutines.newSingleThreadContext
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.runBlockingTest
import kotlinx.coroutines.test.setMain
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.tab.collections.TabCollection
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.TestApplication
import org.mozilla.fenix.ext.components
import org.robolectric.annotation.Config

@ObsoleteCoroutinesApi
@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
@Config(application = TestApplication::class)
class DefaultDeleteBrowsingDataControllerTest {

    private val mainThreadSurrogate = newSingleThreadContext("UI thread")

    private val context: Context = mockk(relaxed = true)
    private lateinit var controller: DefaultDeleteBrowsingDataController

    @Before
    fun setup() {
        Dispatchers.setMain(mainThreadSurrogate)

        every { context.components.core.engine.clearData(any()) } just Runs
    }

    @After
    fun tearDown() {
        Dispatchers.resetMain() // reset main dispatcher to the original Main dispatcher
        mainThreadSurrogate.close()
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
    fun deleteCollections() = runBlockingTest {
        controller = DefaultDeleteBrowsingDataController(context, coroutineContext)

        val collections: List<TabCollection> = listOf(mockk(relaxed = true))
        every { context.components.core.tabCollectionStorage.getTabCollectionsCount() } returns 1

        controller.deleteCollections(collections)

        verify {
            context.components.core.tabCollectionStorage.removeCollection(collections[0])
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

        launch(IO) {
            controller.deleteSitePermissions()
        }

        verify {
            context.components.core.engine.clearData(Engine.BrowsingData.select(Engine.BrowsingData.ALL_SITE_SETTINGS))
            context.components.core.permissionStorage.deleteAllSitePermissions()
        }
    }
}
