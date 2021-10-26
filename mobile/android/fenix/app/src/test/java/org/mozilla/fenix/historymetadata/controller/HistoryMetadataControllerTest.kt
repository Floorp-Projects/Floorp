/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.historymetadata.controller

import androidx.navigation.NavController
import androidx.navigation.NavDirections
import io.mockk.coVerify
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.TestCoroutineScope
import mozilla.components.browser.state.action.HistoryMetadataAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.storage.DocumentType
import mozilla.components.concept.storage.HistoryMetadata
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.concept.storage.HistoryMetadataStorage
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.historymetadata.HistoryMetadataGroup
import org.mozilla.fenix.home.HomeFragmentAction
import org.mozilla.fenix.home.HomeFragmentDirections
import org.mozilla.fenix.home.HomeFragmentStore

@OptIn(ExperimentalCoroutinesApi::class)
class HistoryMetadataControllerTest {

    private val testDispatcher = TestCoroutineDispatcher()

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(testDispatcher)

    private val navController = mockk<NavController>(relaxed = true)
    private val metrics: MetricController = mockk(relaxed = true)

    private lateinit var storage: HistoryMetadataStorage
    private lateinit var homeFragmentStore: HomeFragmentStore
    private lateinit var store: BrowserStore
    private val scope = TestCoroutineScope()

    private lateinit var controller: DefaultHistoryMetadataController

    @Before
    fun setup() {
        every { navController.currentDestination } returns mockk {
            every { id } returns R.id.homeFragment
        }
        storage = mockk(relaxed = true)
        homeFragmentStore = mockk(relaxed = true)
        store = mockk(relaxed = true)

        controller = spyk(
            DefaultHistoryMetadataController(
                homeStore = homeFragmentStore,
                store = store,
                navController = navController,
                scope = scope,
                storage = storage,
                metrics = metrics
            )
        )
    }

    @After
    fun cleanUp() {
        scope.cleanupTestCoroutines()
    }

    @Test
    fun handleHistoryShowAllClicked() {
        controller.handleHistoryShowAllClicked()

        verify {
            controller.dismissSearchDialogIfDisplayed()
            navController.navigate(
                HomeFragmentDirections.actionGlobalHistoryFragment()
            )
        }
    }

    @Test
    fun handleToggleHistoryMetadataGroupClicked() {
        val historyEntry = HistoryMetadata(
            key = HistoryMetadataKey("http://www.mozilla.com", "mozilla", null),
            title = "mozilla",
            createdAt = System.currentTimeMillis(),
            updatedAt = System.currentTimeMillis(),
            totalViewTime = 10,
            documentType = DocumentType.Regular,
            previewImageUrl = null
        )
        val historyGroup = HistoryMetadataGroup(
            title = "mozilla",
            historyMetadata = listOf(historyEntry)
        )

        controller.handleHistoryMetadataGroupClicked(historyGroup)

        verify {
            navController.navigate(
                match<NavDirections> { it.actionId == R.id.action_global_history_metadata_group }
            )
        }
    }

    @Test
    fun handleItemRemoved() {
        val historyMetadataKey = HistoryMetadataKey(
            "http://www.mozilla.com",
            "mozilla",
            null
        )

        val historyGroup = HistoryMetadataGroup(
            title = "mozilla",
            historyMetadata = listOf(
                HistoryMetadata(
                    key = historyMetadataKey,
                    title = "mozilla",
                    createdAt = System.currentTimeMillis(),
                    updatedAt = System.currentTimeMillis(),
                    totalViewTime = 10,
                    documentType = DocumentType.Regular,
                    previewImageUrl = null
                )
            )
        )

        controller.handleRemoveGroup(historyGroup.title)

        testDispatcher.advanceUntilIdle()
        verify {
            store.dispatch(HistoryMetadataAction.DisbandSearchGroupAction(searchTerm = historyGroup.title))
            homeFragmentStore.dispatch(HomeFragmentAction.DisbandSearchGroupAction(searchTerm = historyGroup.title))
            metrics.track(Event.RecentSearchesGroupDeleted)
        }

        coVerify {
            storage.deleteHistoryMetadata(historyGroup.title)
        }
    }
}
