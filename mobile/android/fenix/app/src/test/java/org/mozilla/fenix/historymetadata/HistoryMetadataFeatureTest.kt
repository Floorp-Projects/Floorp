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
import org.junit.After
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

    private val historyEntry = HistoryMetadata(
        key = HistoryMetadataKey("http://www.mozilla.com", "mozilla", null),
        title = "mozilla",
        createdAt = System.currentTimeMillis(),
        updatedAt = System.currentTimeMillis(),
        totalViewTime = 10,
        documentType = DocumentType.Regular
    )
    private val historyGroup = HistoryMetadataGroup(
        title = "mozilla",
        historyMetadata = listOf(historyEntry)
    )

    private val testDispatcher = TestCoroutineDispatcher()

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(testDispatcher)

    @Before
    fun setup() {
        historyMetadataStorage = mockk(relaxed = true)

        coEvery { historyMetadataStorage.getHistoryMetadataSince(any()) }.coAnswers {
            listOf(
                historyEntry
            )
        }
    }

    @After
    fun cleanUp() {
        testDispatcher.cleanupTestCoroutines()
    }

    @Test
    fun `GIVEN no history metadata WHEN feature starts THEN fetch history metadata and notify store`() =
        testDispatcher.runBlockingTest {
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

            middleware.assertLastAction(HomeFragmentAction.HistoryMetadataChange::class) {
                assertEquals(listOf(historyGroup), it.historyMetadata)
            }
        }
}
