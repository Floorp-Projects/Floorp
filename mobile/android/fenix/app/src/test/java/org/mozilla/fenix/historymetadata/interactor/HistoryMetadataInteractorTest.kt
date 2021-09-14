/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.historymetadata.interactor

import androidx.navigation.NavController
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import mozilla.components.concept.storage.DocumentType
import mozilla.components.concept.storage.HistoryMetadata
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.historymetadata.HistoryMetadataGroup
import org.mozilla.fenix.historymetadata.controller.HistoryMetadataController
import org.mozilla.fenix.home.recentbookmarks.controller.RecentBookmarksController
import org.mozilla.fenix.home.recenttabs.controller.RecentTabController
import org.mozilla.fenix.home.sessioncontrol.DefaultSessionControlController
import org.mozilla.fenix.home.sessioncontrol.SessionControlInteractor
import org.mozilla.fenix.home.sessioncontrol.viewholders.pocket.PocketStoriesController

@OptIn(ExperimentalCoroutinesApi::class)
class HistoryMetadataInteractorTest {
    private val testDispatcher = TestCoroutineDispatcher()

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(testDispatcher)

    private val navController = mockk<NavController>(relaxed = true)
    private val defaultSessionControlController: DefaultSessionControlController =
        mockk(relaxed = true)
    private val recentTabController: RecentTabController = mockk(relaxed = true)
    private val recentBookmarksController: RecentBookmarksController = mockk(relaxed = true)
    private val pocketStoriesController: PocketStoriesController = mockk(relaxed = true)
    private val historyMetadataController: HistoryMetadataController = mockk(relaxed = true)

    private lateinit var interactor: SessionControlInteractor

    @Before
    fun setup() {
        every { navController.currentDestination } returns mockk {
            every { id } returns R.id.homeFragment
        }

        interactor = SessionControlInteractor(
            defaultSessionControlController,
            recentTabController,
            recentBookmarksController,
            historyMetadataController,
            pocketStoriesController
        )
    }

    @After
    fun cleanUp() {
        testDispatcher.cleanupTestCoroutines()
    }

    @Test
    fun onHistoryMetadataGroupClicked() {
        val historyGroup =
            HistoryMetadataGroup(
                title = "mozilla",
                historyMetadata = listOf(
                    HistoryMetadata(
                        key = HistoryMetadataKey("http://www.mozilla.com", null, null),
                        title = "mozilla",
                        createdAt = System.currentTimeMillis(),
                        updatedAt = System.currentTimeMillis(),
                        totalViewTime = 10,
                        documentType = DocumentType.Regular,
                        previewImageUrl = null
                    )
                )
            )

        interactor.onHistoryMetadataGroupClicked(historyGroup)
        verify {
            historyMetadataController.handleHistoryMetadataGroupClicked(historyGroup)
        }
    }

    @Test
    fun onHistoryMetadataShowAllClicked() {
        interactor.onHistoryMetadataShowAllClicked()
        verify { historyMetadataController.handleHistoryShowAllClicked() }
    }

    @Test
    fun onRemoveItem() {
        val historyMetadataKey = HistoryMetadataKey(
            "http://www.mozilla.com",
            "mozilla",
            null
        )

        val historyGroup =
            HistoryMetadataGroup(
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

        interactor.onRemoveGroup(historyGroup.title)

        verify {
            historyMetadataController.handleRemoveGroup(historyGroup.title)
        }
    }
}
