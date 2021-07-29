/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.content.ClipData
import android.content.ClipboardManager
import android.content.res.Resources
import androidx.navigation.NavController
import io.mockk.coVerifyOrder
import io.mockk.every
import io.mockk.mockk
import io.mockk.slot
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineScope
import mozilla.components.concept.engine.prompt.ShareData
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.directionsEq
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

// Robolectric needed for `onShareItem()`
@ExperimentalCoroutinesApi
@RunWith(FenixRobolectricTestRunner::class)
class HistoryControllerTest {
    private val historyItem = HistoryItem(0, "title", "url", 0.toLong())
    private val scope = TestCoroutineScope()
    private val store: HistoryFragmentStore = mockk(relaxed = true)
    private val state: HistoryFragmentState = mockk(relaxed = true)
    private val navController: NavController = mockk(relaxed = true)
    private val resources: Resources = mockk(relaxed = true)
    private val snackbar: FenixSnackbar = mockk(relaxed = true)
    private val clipboardManager: ClipboardManager = mockk(relaxed = true)
    private val metrics: MetricController = mockk(relaxed = true)

    @Before
    fun setUp() {
        every { store.state } returns state
    }

    @After
    fun cleanUp() {
        scope.cleanupTestCoroutines()
    }

    @Test
    fun onPressHistoryItemInNormalMode() {
        var actualHistoryItem: HistoryItem? = null
        val controller = createController(
            openInBrowser = {
                actualHistoryItem = it
            }
        )
        controller.handleOpen(historyItem)
        assertEquals(historyItem, actualHistoryItem)
    }

    @Test
    fun onOpenItemInNormalMode() {
        var actualHistoryItem: HistoryItem? = null
        var actualBrowsingMode: BrowsingMode? = null
        val controller = createController(
            openAndShowTray = { historyItem, browsingMode ->
                actualHistoryItem = historyItem
                actualBrowsingMode = browsingMode
            }
        )
        controller.handleOpenInNewTab(historyItem, BrowsingMode.Normal)
        assertEquals(historyItem, actualHistoryItem)
        assertEquals(BrowsingMode.Normal, actualBrowsingMode)
    }

    @Test
    fun onOpenItemInPrivateMode() {
        var actualHistoryItem: HistoryItem? = null
        var actualBrowsingMode: BrowsingMode? = null
        val controller = createController(
            openAndShowTray = { historyItem, browsingMode ->
                actualHistoryItem = historyItem
                actualBrowsingMode = browsingMode
            }
        )
        controller.handleOpenInNewTab(historyItem, BrowsingMode.Private)
        assertEquals(historyItem, actualHistoryItem)
        assertEquals(BrowsingMode.Private, actualBrowsingMode)
    }

    @Test
    fun onPressHistoryItemInEditMode() {
        every { state.mode } returns HistoryFragmentState.Mode.Editing(setOf())

        createController().handleSelect(historyItem)

        verify {
            store.dispatch(HistoryFragmentAction.AddItemForRemoval(historyItem))
        }
    }

    @Test
    fun onPressSelectedHistoryItemInEditMode() {
        every { state.mode } returns HistoryFragmentState.Mode.Editing(setOf(historyItem))

        createController().handleDeselect(historyItem)

        verify {
            store.dispatch(HistoryFragmentAction.RemoveItemForRemoval(historyItem))
        }
    }

    @Test
    fun onSelectHistoryItemDuringSync() {
        every { state.mode } returns HistoryFragmentState.Mode.Syncing

        createController().handleSelect(historyItem)

        verify(exactly = 0) {
            store.dispatch(HistoryFragmentAction.AddItemForRemoval(historyItem))
        }
    }

    @Test
    fun onBackPressedInNormalMode() {
        every { state.mode } returns HistoryFragmentState.Mode.Normal

        assertFalse(createController().handleBackPressed())
    }

    @Test
    fun onBackPressedInEditMode() {
        every { state.mode } returns HistoryFragmentState.Mode.Editing(setOf())

        assertTrue(createController().handleBackPressed())
        verify {
            store.dispatch(HistoryFragmentAction.ExitEditMode)
        }
    }

    @Test
    fun onModeSwitched() {
        var invalidateOptionsMenuInvoked = false
        val controller = createController(
            invalidateOptionsMenu = {
                invalidateOptionsMenuInvoked = true
            }
        )

        controller.handleModeSwitched()
        assertTrue(invalidateOptionsMenuInvoked)
    }

    @Test
    fun onDeleteAll() {
        var displayDeleteAllInvoked = false
        val controller = createController(
            displayDeleteAll = {
                displayDeleteAllInvoked = true
            }
        )

        controller.handleDeleteAll()
        assertTrue(displayDeleteAllInvoked)
    }

    @Test
    fun onDeleteSome() {
        val itemsToDelete = setOf(historyItem)
        var actualItems: Set<HistoryItem>? = null
        val controller = createController(
            deleteHistoryItems = { items ->
                actualItems = items
            }
        )

        controller.handleDeleteSome(itemsToDelete)
        assertEquals(itemsToDelete, actualItems)
    }

    @Test
    fun onCopyItem() {
        val clipdata = slot<ClipData>()

        createController().handleCopyUrl(historyItem)

        verify {
            clipboardManager.setPrimaryClip(capture(clipdata))
            snackbar.show()
        }
        assertEquals(1, clipdata.captured.itemCount)
        assertEquals(historyItem.url, clipdata.captured.description.label)
        assertEquals(historyItem.url, clipdata.captured.getItemAt(0).text)
    }

    @Test
    @Suppress("UNCHECKED_CAST")
    fun onShareItem() {
        createController().handleShare(historyItem)

        verify {
            navController.navigate(
                directionsEq(
                    HistoryFragmentDirections.actionGlobalShareFragment(
                        data = arrayOf(ShareData(url = historyItem.url, title = historyItem.title))
                    )
                )
            )
        }
    }

    @Test
    fun onRequestSync() {
        var syncHistoryInvoked = false
        createController(
            syncHistory = {
                syncHistoryInvoked = true
            }
        ).handleRequestSync()

        coVerifyOrder {
            store.dispatch(HistoryFragmentAction.StartSync)
            store.dispatch(HistoryFragmentAction.FinishSync)
        }

        assertTrue(syncHistoryInvoked)
    }

    @Suppress("LongParameterList")
    private fun createController(
        openInBrowser: (HistoryItem) -> Unit = { _ -> },
        openAndShowTray: (HistoryItem, BrowsingMode) -> Unit = { _, _ -> },
        displayDeleteAll: () -> Unit = { },
        invalidateOptionsMenu: () -> Unit = { },
        deleteHistoryItems: (Set<HistoryItem>) -> Unit = { _ -> },
        syncHistory: suspend () -> Unit = { }
    ): HistoryController {
        return DefaultHistoryController(
            store,
            navController,
            resources,
            snackbar,
            clipboardManager,
            scope,
            openInBrowser,
            openAndShowTray,
            displayDeleteAll,
            invalidateOptionsMenu,
            deleteHistoryItems,
            syncHistory,
            metrics
        )
    }
}
