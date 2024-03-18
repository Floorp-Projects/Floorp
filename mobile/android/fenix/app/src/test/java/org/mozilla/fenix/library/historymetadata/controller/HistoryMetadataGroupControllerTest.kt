/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.historymetadata.controller

import android.content.Context
import androidx.navigation.NavController
import io.mockk.coVerify
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.launch
import mozilla.components.browser.state.action.HistoryMetadataAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.directionsEq
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.library.history.History
import org.mozilla.fenix.library.history.HistoryItemTimeGroup
import org.mozilla.fenix.library.historymetadata.HistoryMetadataGroupFragmentAction
import org.mozilla.fenix.library.historymetadata.HistoryMetadataGroupFragmentDirections
import org.mozilla.fenix.library.historymetadata.HistoryMetadataGroupFragmentStore
import org.mozilla.fenix.GleanMetrics.History as GleanHistory

@RunWith(FenixRobolectricTestRunner::class)
class HistoryMetadataGroupControllerTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    private val activity: HomeActivity = mockk(relaxed = true)
    private val context: Context = mockk(relaxed = true)
    private val appStore: AppStore = mockk(relaxed = true)
    private val store: HistoryMetadataGroupFragmentStore = mockk(relaxed = true)
    private val browserStore: BrowserStore = mockk(relaxed = true)
    private val selectOrAddUseCase: TabsUseCases.SelectOrAddUseCase = mockk(relaxed = true)
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
        historyMetadataKey = historyMetadataKey,
    )
    private val firefoxHistoryMetadataItem = History.Metadata(
        position = 1,
        title = "Firefox",
        url = "firefox.com",
        visitedAt = 0,
        historyTimeGroup = HistoryItemTimeGroup.timeGroupForTimestamp(0),
        totalViewTime = 1,
        historyMetadataKey = historyMetadataKey,
    )

    private lateinit var controller: DefaultHistoryMetadataGroupController

    private fun getMetadataItemsList() =
        listOf(mozillaHistoryMetadataItem, firefoxHistoryMetadataItem)

    @Before
    fun setUp() {
        controller = createController()
        every { activity.components.core.historyStorage } returns historyStorage
        every { context.components.core.store } returns browserStore
        every { context.components.core.historyStorage } returns historyStorage
        every { store.state.items } returns getMetadataItemsList()
    }

    @Test
    fun handleOpen() {
        assertNull(GleanHistory.searchTermGroupOpenTab.testGetValue())

        controller.handleOpen(mozillaHistoryMetadataItem)

        verify {
            selectOrAddUseCase.invoke(
                mozillaHistoryMetadataItem.url,
                mozillaHistoryMetadataItem.historyMetadataKey,
            )
            navController.navigate(R.id.browserFragment)
        }
        assertNotNull(GleanHistory.searchTermGroupOpenTab.testGetValue())
        assertEquals(
            1,
            GleanHistory.searchTermGroupOpenTab.testGetValue()!!.size,
        )
        assertNull(
            GleanHistory.searchTermGroupOpenTab.testGetValue()!!
                .single().extra,
        )
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
                url = mozillaHistoryMetadataItem.url,
            ),
            ShareData(
                title = firefoxHistoryMetadataItem.title,
                url = firefoxHistoryMetadataItem.url,
            ),
        )

        verify {
            navController.navigate(
                directionsEq(HistoryMetadataGroupFragmentDirections.actionGlobalShareFragment(data)),
            )
        }
    }

    @Test
    fun handleDeleteSingle() = runTestOnMain {
        assertNull(GleanHistory.searchTermGroupRemoveTab.testGetValue())

        controller.handleDelete(setOf(mozillaHistoryMetadataItem))

        coVerify {
            store.dispatch(HistoryMetadataGroupFragmentAction.Delete(mozillaHistoryMetadataItem))
            historyStorage.deleteVisitsFor(mozillaHistoryMetadataItem.url)
        }
        assertNotNull(GleanHistory.searchTermGroupRemoveTab.testGetValue())
        assertEquals(
            1,
            GleanHistory.searchTermGroupRemoveTab.testGetValue()!!.size,
        )
        assertNull(
            GleanHistory.searchTermGroupRemoveTab.testGetValue()!!
                .single().extra,
        )
        // Here we don't expect the action to be dispatched, because items inside the store
        // we provided by getMetadataItemsList(), but only one item has been removed
        verify(exactly = 0) {
            browserStore.dispatch(
                HistoryMetadataAction.DisbandSearchGroupAction(searchTerm = searchTerm),
            )
        }
    }

    @Test
    fun handleDeleteMultiple() = runTestOnMain {
        assertNull(GleanHistory.searchTermGroupRemoveTab.testGetValue())
        controller.handleDelete(getMetadataItemsList().toSet())

        coVerify {
            getMetadataItemsList().forEach {
                store.dispatch(HistoryMetadataGroupFragmentAction.Delete(it))
                historyStorage.deleteVisitsFor(it.url)
            }
        }
        assertNotNull(GleanHistory.searchTermGroupRemoveTab.testGetValue())
        assertNull(
            GleanHistory.searchTermGroupRemoveTab.testGetValue()!!
                .last().extra,
        )
        // Here we expect the action to be dispatched, because both deleted items and items inside
        // the store were provided by the same method getMetadataItemsList()
        verify {
            browserStore.dispatch(
                HistoryMetadataAction.DisbandSearchGroupAction(searchTerm = searchTerm),
            )
        }
    }

    @Test
    fun handleDeleteAbnormal() = runTestOnMain {
        val abnormalList = listOf(
            mozillaHistoryMetadataItem,
            firefoxHistoryMetadataItem,
            mozillaHistoryMetadataItem.copy(title = "Pocket", url = "https://getpocket.com"),
            mozillaHistoryMetadataItem.copy(title = "BBC", url = "https://www.bbc.com/"),
            mozillaHistoryMetadataItem.copy(
                title = "Stackoverflow",
                url = "https://stackoverflow.com/",
            ),
        )
        assertNull(GleanHistory.searchTermGroupRemoveTab.testGetValue())

        controller.handleDelete(abnormalList.toSet())
        coVerify {
            getMetadataItemsList().forEach {
                store.dispatch(HistoryMetadataGroupFragmentAction.Delete(it))
                historyStorage.deleteVisitsFor(it.url)
            }
        }
        assertNotNull(GleanHistory.searchTermGroupRemoveTab.testGetValue())
        assertNull(
            GleanHistory.searchTermGroupRemoveTab.testGetValue()!!
                .last().extra,
        )
        coVerify {
            abnormalList.forEach {
                store.dispatch(HistoryMetadataGroupFragmentAction.Delete(it))
                historyStorage.deleteVisitsFor(it.url)
            }
        }
        assertNotNull(GleanHistory.searchTermGroupRemoveTab.testGetValue())
        assertNull(
            GleanHistory.searchTermGroupRemoveTab.testGetValue()!!
                .last().extra,
        )
        // Here we expect the action to be dispatched, because deleted items include the items
        // provided by getMetadataItemsList(), so that the store becomes empty and the event
        // should be sent
        verify {
            browserStore.dispatch(
                HistoryMetadataAction.DisbandSearchGroupAction(searchTerm = searchTerm),
            )
        }
    }

    @Test
    fun handleDeleteAll() = runTestOnMain {
        var promptDeleteAllInvoked = false
        val controller = createController(
            promptDeleteAll = {
                promptDeleteAllInvoked = true
            },
        )
        controller.handleDeleteAll()
        assertTrue(promptDeleteAllInvoked)
    }

    @Test
    fun handleDeleteAllConfirmed() = runTestOnMain {
        assertNull(GleanHistory.searchTermGroupRemoveAll.testGetValue())

        controller.handleDeleteAllConfirmed()

        coVerify {
            store.dispatch(HistoryMetadataGroupFragmentAction.DeleteAll)
            getMetadataItemsList().forEach {
                historyStorage.deleteVisitsFor(it.url)
            }
            browserStore.dispatch(
                HistoryMetadataAction.DisbandSearchGroupAction(searchTerm = searchTerm),
            )
        }
        assertNotNull(GleanHistory.searchTermGroupRemoveAll.testGetValue())
        assertEquals(
            1,
            GleanHistory.searchTermGroupRemoveAll.testGetValue()!!.size,
        )
        assertNull(
            GleanHistory.searchTermGroupRemoveAll.testGetValue()!!
                .single().extra,
        )
    }

    private fun createController(
        deleteSnackbar: (
            items: Set<History.Metadata>,
            undo: suspend (Set<History.Metadata>) -> Unit,
            delete: (Set<History.Metadata>) -> suspend (context: Context) -> Unit,
        ) -> Unit = { items, _, delete ->
            scope.launch {
                delete(items).invoke(context)
            }
        },
        promptDeleteAll: () -> Unit = {},
        allDeletedSnackbar: () -> Unit = {},
    ): DefaultHistoryMetadataGroupController {
        return DefaultHistoryMetadataGroupController(
            historyStorage = historyStorage,
            browserStore = browserStore,
            appStore = appStore,
            store = store,
            selectOrAddUseCase = selectOrAddUseCase,
            navController = navController,
            scope = scope,
            searchTerm = searchTerm,
            deleteSnackbar = deleteSnackbar,
            promptDeleteAll = promptDeleteAll,
            allDeletedSnackbar = allDeletedSnackbar,
        )
    }
}
