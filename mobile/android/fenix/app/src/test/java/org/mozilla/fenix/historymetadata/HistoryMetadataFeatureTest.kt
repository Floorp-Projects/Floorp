/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.historymetadata

import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.mockk
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.concept.storage.DocumentType
import mozilla.components.concept.storage.HistoryMetadata
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.concept.storage.HistoryMetadataStorage
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.home.HomeFragmentAction
import org.mozilla.fenix.home.HomeFragmentState
import org.mozilla.fenix.home.HomeFragmentStore

@OptIn(ExperimentalCoroutinesApi::class)
class HistoryMetadataFeatureTest {

    private lateinit var historyMetadataStorage: HistoryMetadataStorage

    private val middleware = CaptureActionsMiddleware<HomeFragmentState, HomeFragmentAction>()
    private val homeStore = HomeFragmentStore(middlewares = listOf(middleware))
    private val testDispatcher = TestCoroutineDispatcher()

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(testDispatcher)

    @Before
    fun setup() {
        historyMetadataStorage = mockk(relaxed = true)
    }

    @Test
    fun `GIVEN no history metadata WHEN feature starts THEN fetch history metadata and notify store`() =
        testDispatcher.runBlockingTest {
            val historyEntry = HistoryMetadata(
                key = HistoryMetadataKey("http://www.mozilla.com", "mozilla", null),
                title = "mozilla",
                createdAt = System.currentTimeMillis(),
                updatedAt = System.currentTimeMillis(),
                totalViewTime = 10,
                documentType = DocumentType.Regular,
                previewImageUrl = null
            )
            val expectedHistoryGroup = HistoryMetadataGroup(
                title = "mozilla",
                historyMetadata = listOf(historyEntry)
            )

            coEvery { historyMetadataStorage.getHistoryMetadataSince(any()) }.coAnswers {
                listOf(
                    historyEntry
                )
            }

            startHistoryMetadataFeature()

            middleware.assertLastAction(HomeFragmentAction.HistoryMetadataChange::class) {
                assertEquals(listOf(expectedHistoryGroup), it.historyMetadata)
            }
        }

    @Test
    fun `GIVEN history metadata WHEN group contains multiple entries with same url THEN entries are deduped`() =
        testDispatcher.runBlockingTest {
            val historyEntry1 = HistoryMetadata(
                key = HistoryMetadataKey("http://www.mozilla.com", "mozilla", null),
                title = "mozilla",
                createdAt = System.currentTimeMillis(),
                updatedAt = 1,
                totalViewTime = 10,
                documentType = DocumentType.Regular,
                previewImageUrl = null
            )

            val historyEntry2 = HistoryMetadata(
                key = HistoryMetadataKey("http://firefox.com", "mozilla", null),
                title = "firefox",
                createdAt = System.currentTimeMillis(),
                updatedAt = 2,
                totalViewTime = 20,
                documentType = DocumentType.Regular,
                previewImageUrl = "http://firefox.com/image1"
            )

            val historyEntry3 = HistoryMetadata(
                key = HistoryMetadataKey("http://www.mozilla.com", "mozilla", null),
                title = "mozilla",
                createdAt = System.currentTimeMillis(),
                updatedAt = 3,
                totalViewTime = 30,
                documentType = DocumentType.Regular,
                previewImageUrl = null
            )

            val expectedHistoryGroup = HistoryMetadataGroup(
                title = "mozilla",
                historyMetadata = listOf(
                    // Expected total view time to be summed up for deduped entries
                    historyEntry1.copy(
                        totalViewTime = historyEntry1.totalViewTime + historyEntry3.totalViewTime,
                        updatedAt = historyEntry3.updatedAt
                    ),
                    historyEntry2
                )
            )

            coEvery { historyMetadataStorage.getHistoryMetadataSince(any()) }.coAnswers {
                listOf(
                    historyEntry1, historyEntry2, historyEntry3
                )
            }

            startHistoryMetadataFeature()

            middleware.assertLastAction(HomeFragmentAction.HistoryMetadataChange::class) {
                assertEquals(listOf(expectedHistoryGroup), it.historyMetadata)
            }
        }

    @Test
    fun `GIVEN history metadata WHEN different groups contain entries with same url THEN entries are not deduped`() =
        testDispatcher.runBlockingTest {
            val historyEntry1 = HistoryMetadata(
                key = HistoryMetadataKey("http://www.mozilla.com", "mozilla", null),
                title = "mozilla",
                createdAt = System.currentTimeMillis(),
                updatedAt = System.currentTimeMillis(),
                totalViewTime = 10,
                documentType = DocumentType.Regular,
                previewImageUrl = null
            )

            val historyEntry2 = HistoryMetadata(
                key = HistoryMetadataKey("http://firefox.com", "mozilla", null),
                title = "firefox",
                createdAt = System.currentTimeMillis(),
                updatedAt = System.currentTimeMillis(),
                totalViewTime = 20,
                documentType = DocumentType.Regular,
                previewImageUrl = null
            )

            val historyEntry3 = HistoryMetadata(
                key = HistoryMetadataKey("http://www.mozilla.com", "firefox", null),
                title = "mozilla",
                createdAt = System.currentTimeMillis(),
                updatedAt = System.currentTimeMillis(),
                totalViewTime = 30,
                documentType = DocumentType.Regular,
                previewImageUrl = null
            )

            val expectedHistoryGroup1 = HistoryMetadataGroup(
                title = "mozilla",
                historyMetadata = listOf(historyEntry1, historyEntry2)
            )

            val expectedHistoryGroup2 = HistoryMetadataGroup(
                title = "firefox",
                historyMetadata = listOf(historyEntry3)
            )

            coEvery { historyMetadataStorage.getHistoryMetadataSince(any()) }.coAnswers {
                listOf(
                    historyEntry1, historyEntry2, historyEntry3
                )
            }

            startHistoryMetadataFeature()

            middleware.assertLastAction(HomeFragmentAction.HistoryMetadataChange::class) {
                assertEquals(listOf(expectedHistoryGroup1, expectedHistoryGroup2), it.historyMetadata)
            }
        }

    private fun startHistoryMetadataFeature() {
        val feature = HistoryMetadataFeature(
            homeStore,
            historyMetadataStorage,
            CoroutineScope(testDispatcher),
            testDispatcher
        )

        assertEquals(emptyList<HistoryMetadataGroup>(), homeStore.state.historyMetadata)

        feature.start()

        testDispatcher.advanceUntilIdle()
        homeStore.waitUntilIdle()

        coVerify {
            historyMetadataStorage.getHistoryMetadataSince(any())
        }
    }
}
