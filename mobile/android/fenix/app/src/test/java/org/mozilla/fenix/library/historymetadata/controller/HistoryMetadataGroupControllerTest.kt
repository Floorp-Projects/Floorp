/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.historymetadata.controller

import androidx.navigation.NavController
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.ext.directionsEq
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.library.history.History
import org.mozilla.fenix.library.historymetadata.HistoryMetadataGroupFragmentAction
import org.mozilla.fenix.library.historymetadata.HistoryMetadataGroupFragmentDirections
import org.mozilla.fenix.library.historymetadata.HistoryMetadataGroupFragmentStore

@ExperimentalCoroutinesApi
@RunWith(FenixRobolectricTestRunner::class)
class HistoryMetadataGroupControllerTest {

    private val testDispatcher = TestCoroutineDispatcher()

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(testDispatcher)

    private val activity: HomeActivity = mockk(relaxed = true)
    private val store: HistoryMetadataGroupFragmentStore = mockk(relaxed = true)
    private val navController: NavController = mockk(relaxed = true)

    private val mozillaHistoryMetadataItem = History.Metadata(
        id = 0,
        title = "Mozilla",
        url = "mozilla.org",
        visitedAt = 0,
        totalViewTime = 1,
        historyMetadataKey = HistoryMetadataKey("http://www.mozilla.com", "mozilla", null)
    )
    private val firefoxHistoryMetadataItem = History.Metadata(
        id = 0,
        title = "Firefox",
        url = "firefox.com",
        visitedAt = 0,
        totalViewTime = 1,
        historyMetadataKey = HistoryMetadataKey("http://www.firefox.com", "mozilla", null)
    )

    private lateinit var controller: DefaultHistoryMetadataGroupController

    @Before
    fun setUp() {
        controller = DefaultHistoryMetadataGroupController(
            activity = activity,
            store = store,
            navController = navController
        )
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
}
