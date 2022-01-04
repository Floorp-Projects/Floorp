/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.historymetadata.controller

import androidx.navigation.NavController
import io.mockk.coVerify
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.test.TestCoroutineScope
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.directionsEq
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.library.history.History
import org.mozilla.fenix.library.historymetadata.HistoryMetadataGroupFragmentAction
import org.mozilla.fenix.library.historymetadata.HistoryMetadataGroupFragmentDirections
import org.mozilla.fenix.library.historymetadata.HistoryMetadataGroupFragmentStore

@RunWith(FenixRobolectricTestRunner::class)
class HistoryMetadataGroupControllerTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val testDispatcher = coroutinesTestRule.testDispatcher
    private val scope = TestCoroutineScope(testDispatcher)

    private val activity: HomeActivity = mockk(relaxed = true)
    private val store: HistoryMetadataGroupFragmentStore = mockk(relaxed = true)
    private val metrics: MetricController = mockk(relaxed = true)
    private val navController: NavController = mockk(relaxed = true)
    private val historyStorage: PlacesHistoryStorage = mockk(relaxed = true)

    private val searchTerm = "mozilla"
    private val historyMetadataKey = HistoryMetadataKey("http://www.mozilla.com", searchTerm, null)
    private val mozillaHistoryMetadataItem = History.Metadata(
        position = 1,
        title = "Mozilla",
        url = "mozilla.org",
        visitedAt = 0,
        totalViewTime = 1,
        historyMetadataKey = historyMetadataKey
    )
    private val firefoxHistoryMetadataItem = History.Metadata(
        position = 1,
        title = "Firefox",
        url = "firefox.com",
        visitedAt = 0,
        totalViewTime = 1,
        historyMetadataKey = historyMetadataKey
    )

    private lateinit var controller: DefaultHistoryMetadataGroupController

    @Before
    fun setUp() {
        controller = DefaultHistoryMetadataGroupController(
            activity = activity,
            store = store,
            metrics = metrics,
            navController = navController,
            scope = scope,
            searchTerm = "mozilla"
        )

        every { activity.components.core.historyStorage } returns historyStorage
    }

    @After
    fun cleanUp() {
        scope.cleanupTestCoroutines()
    }

    @Test
    fun handleOpen() {
        controller.handleOpen(mozillaHistoryMetadataItem)

        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = mozillaHistoryMetadataItem.url,
                newTab = true,
                from = BrowserDirection.FromHistoryMetadataGroup,
                historyMetadata = mozillaHistoryMetadataItem.historyMetadataKey
            )
            metrics.track(Event.HistorySearchTermGroupOpenTab)
        }
    }

    @Test
    fun handleSelect() {
        controller.handleSelect(mozillaHistoryMetadataItem)

        verify {
            store.dispatch(HistoryMetadataGroupFragmentAction.Select(mozillaHistoryMetadataItem))
        }
    }

    @Test
    fun handleDeselect() {
        controller.handleDeselect(mozillaHistoryMetadataItem)

        verify {
            store.dispatch(HistoryMetadataGroupFragmentAction.Deselect(mozillaHistoryMetadataItem))
        }
    }

    @Test
    fun handleBackPressed() {
        assertTrue(controller.handleBackPressed(setOf(mozillaHistoryMetadataItem)))

        verify {
            store.dispatch(HistoryMetadataGroupFragmentAction.DeselectAll)
        }

        assertFalse(controller.handleBackPressed(emptySet()))
    }

    @Test
    fun handleShare() {
        controller.handleShare(setOf(mozillaHistoryMetadataItem, firefoxHistoryMetadataItem))

        val data = arrayOf(
            ShareData(
                title = mozillaHistoryMetadataItem.title,
                url = mozillaHistoryMetadataItem.url
            ),
            ShareData(
                title = firefoxHistoryMetadataItem.title,
                url = firefoxHistoryMetadataItem.url
            ),
        )

        verify {
            navController.navigate(
                directionsEq(HistoryMetadataGroupFragmentDirections.actionGlobalShareFragment(data))
            )
        }
    }

    @Test
    fun handleDelete() = testDispatcher.runBlockingTest {
        controller.handleDelete(setOf(mozillaHistoryMetadataItem, firefoxHistoryMetadataItem))

        coVerify {
            store.dispatch(HistoryMetadataGroupFragmentAction.Delete(mozillaHistoryMetadataItem))
            store.dispatch(HistoryMetadataGroupFragmentAction.Delete(firefoxHistoryMetadataItem))
            historyStorage.deleteHistoryMetadata(mozillaHistoryMetadataItem.historyMetadataKey)
            historyStorage.deleteHistoryMetadata(firefoxHistoryMetadataItem.historyMetadataKey)
            metrics.track(Event.HistorySearchTermGroupRemoveTab)
        }
    }

    @Test
    fun handleDeleteAll() = testDispatcher.runBlockingTest {
        controller.handleDeleteAll()

        coVerify {
            store.dispatch(HistoryMetadataGroupFragmentAction.DeleteAll)
            historyStorage.deleteHistoryMetadata(searchTerm)
            metrics.track(Event.HistorySearchTermGroupRemoveAll)
        }
    }
}
