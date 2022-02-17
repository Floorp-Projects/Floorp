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
import mozilla.components.browser.state.action.HistoryMetadataAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.directionsEq
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.library.history.History
import org.mozilla.fenix.library.history.HistoryItemTimeGroup
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
    private val browserStore: BrowserStore = mockk(relaxed = true)
    private val selectOrAddUseCase: TabsUseCases.SelectOrAddUseCase = mockk(relaxed = true)
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
        historyTimeGroup = HistoryItemTimeGroup.timeGroupForTimestamp(0),
        totalViewTime = 1,
        historyMetadataKey = historyMetadataKey
    )
    private val firefoxHistoryMetadataItem = History.Metadata(
        position = 1,
        title = "Firefox",
        url = "firefox.com",
        visitedAt = 0,
        historyTimeGroup = HistoryItemTimeGroup.timeGroupForTimestamp(0),
        totalViewTime = 1,
        historyMetadataKey = historyMetadataKey
    )

    private lateinit var controller: DefaultHistoryMetadataGroupController

    private fun getMetadataItemsList() =
        listOf(mozillaHistoryMetadataItem, firefoxHistoryMetadataItem)

    @Before
    fun setUp() {
        controller = DefaultHistoryMetadataGroupController(
            historyStorage = historyStorage,
            browserStore = browserStore,
            store = store,
            selectOrAddUseCase = selectOrAddUseCase,
            metrics = metrics,
            navController = navController,
            scope = scope,
            searchTerm = "mozilla"
        )

        every { activity.components.core.historyStorage } returns historyStorage
        every { store.state.items } returns getMetadataItemsList()
    }

    @After
    fun cleanUp() {
        scope.cleanupTestCoroutines()
    }

    @Test
    fun handleOpen() {
        controller.handleOpen(mozillaHistoryMetadataItem)

        verify {
            selectOrAddUseCase.invoke(
                mozillaHistoryMetadataItem.url,
                mozillaHistoryMetadataItem.historyMetadataKey
            )
            navController.navigate(R.id.browserFragment)
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
    fun handleDeleteSingle() = testDispatcher.runBlockingTest {
        controller.handleDelete(setOf(mozillaHistoryMetadataItem))

        coVerify {
            store.dispatch(HistoryMetadataGroupFragmentAction.Delete(mozillaHistoryMetadataItem))
            historyStorage.deleteVisitsFor(mozillaHistoryMetadataItem.url)
            metrics.track(Event.HistorySearchTermGroupRemoveTab)
        }
        // Here we don't expect the action to be dispatched, because items inside the store
        // we provided by getMetadataItemsList(), but only one item has been removed
        verify(exactly = 0) {
            browserStore.dispatch(
                HistoryMetadataAction.DisbandSearchGroupAction(searchTerm = searchTerm)
            )
        }
    }

    @Test
    fun handleDeleteMultiple() = testDispatcher.runBlockingTest {
        controller.handleDelete(getMetadataItemsList().toSet())

        coVerify {
            getMetadataItemsList().forEach {
                store.dispatch(HistoryMetadataGroupFragmentAction.Delete(it))
                historyStorage.deleteVisitsFor(it.url)
            }
            metrics.track(Event.HistorySearchTermGroupRemoveTab)
        }
        // Here we expect the action to be dispatched, because both deleted items and items inside
        // the store were provided by the same method getMetadataItemsList()
        verify {
            browserStore.dispatch(
                HistoryMetadataAction.DisbandSearchGroupAction(searchTerm = searchTerm)
            )
        }
    }

    @Test
    fun handleDeleteAbnormal() = testDispatcher.runBlockingTest {
        val abnormalList = listOf(
            mozillaHistoryMetadataItem,
            firefoxHistoryMetadataItem,
            mozillaHistoryMetadataItem.copy(title = "Pocket", url = "https://getpocket.com"),
            mozillaHistoryMetadataItem.copy(title = "BBC", url = "https://www.bbc.com/"),
            mozillaHistoryMetadataItem.copy(title = "Stackoverflow", url = "https://stackoverflow.com/")
        )
        controller.handleDelete(abnormalList.toSet())
        coVerify {
            getMetadataItemsList().forEach {
                store.dispatch(HistoryMetadataGroupFragmentAction.Delete(it))
                historyStorage.deleteVisitsFor(it.url)
            }
            metrics.track(Event.HistorySearchTermGroupRemoveTab)
        }
        // Here we expect the action to be dispatched, because deleted items include the items
        // provided by getMetadataItemsList(), so that the store becomes empty and the event
        // should be sent
        verify {
            browserStore.dispatch(
                HistoryMetadataAction.DisbandSearchGroupAction(searchTerm = searchTerm)
            )
        }
    }

    @Test
    fun handleDeleteAll() = testDispatcher.runBlockingTest {
        controller.handleDeleteAll()

        coVerify {
            store.dispatch(HistoryMetadataGroupFragmentAction.DeleteAll)
            getMetadataItemsList().forEach {
                historyStorage.deleteVisitsFor(it.url)
            }
            browserStore.dispatch(
                HistoryMetadataAction.DisbandSearchGroupAction(searchTerm = searchTerm)
            )
            metrics.track(Event.HistorySearchTermGroupRemoveAll)
        }
    }
}
