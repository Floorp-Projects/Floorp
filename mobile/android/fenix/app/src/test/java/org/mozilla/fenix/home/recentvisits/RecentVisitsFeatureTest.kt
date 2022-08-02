/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentvisits

import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.every
import io.mockk.mockk
import io.mockk.slot
import io.mockk.spyk
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.advanceUntilIdle
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.concept.storage.DocumentType
import mozilla.components.concept.storage.HistoryHighlight
import mozilla.components.concept.storage.HistoryHighlightWeights
import mozilla.components.concept.storage.HistoryMetadata
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.concept.storage.HistoryMetadataStorage
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem.RecentHistoryHighlight
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItemInternal.HistoryHighlightInternal
import kotlin.random.Random

@OptIn(ExperimentalCoroutinesApi::class)
class RecentVisitsFeatureTest {

    private lateinit var historyHightlightsStorage: PlacesHistoryStorage
    private lateinit var historyMetadataStorage: HistoryMetadataStorage

    private val middleware = CaptureActionsMiddleware<AppState, AppAction>()
    private val appStore = AppStore(middlewares = listOf(middleware))

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val testDispatcher = coroutinesTestRule.testDispatcher
    private val scope = coroutinesTestRule.scope

    @Before
    fun setup() {
        historyHightlightsStorage = mockk(relaxed = true)
        historyMetadataStorage = mockk(relaxed = true)
    }

    @Test
    fun `GIVEN no recent visits WHEN feature starts THEN fetch history metadata and highlights then notify store`() =
        runTestOnMain {
            val historyEntry = HistoryMetadata(
                key = HistoryMetadataKey("http://www.mozilla.com", "mozilla", null),
                title = "mozilla",
                createdAt = System.currentTimeMillis(),
                updatedAt = System.currentTimeMillis(),
                totalViewTime = 10,
                documentType = DocumentType.Regular,
                previewImageUrl = null
            )
            val highlightEntry = HistoryHighlight(1.0, 1, "https://firefox.com", "firefox", null)
            val recentHistoryHighlight = RecentHistoryHighlight("firefox", "https://firefox.com")
            coEvery { historyMetadataStorage.getHistoryMetadataSince(any()) }.coAnswers {
                listOf(
                    historyEntry
                )
            }
            coEvery { historyHightlightsStorage.getHistoryHighlights(any(), any()) }.coAnswers {
                listOf(highlightEntry)
            }

            startRecentVisitsFeature()

            middleware.assertLastAction(AppAction.RecentHistoryChange::class) {
                assertEquals(listOf(recentHistoryHighlight), it.recentHistory)
            }
        }

    @Test
    fun `WHEN asking for history highlights THEN use a specific query`() {
        runTestOnMain {
            val highlightWeights = slot<HistoryHighlightWeights>()
            val highlightsAskedForNumber = slot<Int>()

            startRecentVisitsFeature()

            coVerify {
                historyHightlightsStorage.getHistoryHighlights(
                    capture(highlightWeights),
                    capture(highlightsAskedForNumber)
                )
            }

            assertEquals(MIN_VIEW_TIME_OF_HIGHLIGHT, highlightWeights.captured.viewTime, 0.0)
            assertEquals(MIN_FREQUENCY_OF_HIGHLIGHT, highlightWeights.captured.frequency, 0.0)
            assertEquals(MAX_RESULTS_TOTAL, highlightsAskedForNumber.captured)
        }
    }

    @Test
    fun `GIVEN multiple highlights exist WHEN they are added to store THEN only MAX_RESULTS_TOTAL are sent`() =
        runTestOnMain {
            val highlights = getHistoryHighlightsItems(10)
            val expectedRecentHighlights = highlights
                // Expect to only have 9 highlights
                .subList(0, 9)
                .toRecentHistoryHighlights()
            coEvery { historyHightlightsStorage.getHistoryHighlights(any(), any()) }.coAnswers { highlights }

            startRecentVisitsFeature()

            middleware.assertLastAction(AppAction.RecentHistoryChange::class) {
                assertEquals(
                    expectedRecentHighlights,
                    it.recentHistory
                )
            }
        }

    @Test
    fun `GIVEN a list of history highlights WHEN updateState is called THEN emit RecentHistoryChange`() {
        val feature = spyk(RecentVisitsFeature(appStore, mockk(), mockk(), mockk(), mockk()))
        val expected = List<RecentHistoryHighlight>(1) { mockk() }
        every { feature.getCombinedHistory(any()) } returns expected

        feature.updateState(emptyList())
        appStore.waitUntilIdle()

        middleware.assertLastAction(AppAction.RecentHistoryChange::class) {
            assertEquals(expected, it.recentHistory)
        }
    }

    @Test
    fun `GIVEN all highlights have metadata WHEN getHistoryHighlights is called THEN return a list of highlights with an inferred last access time`() {
        val feature = RecentVisitsFeature(mockk(), mockk(), mockk(), mockk(), mockk())
        val visitsFromSearch = getSearchFromHistoryMetadataItems(10)
        val directVisits = getDirectVisitsHistoryMetadataItems(10)

        val result = feature.getHistoryHighlights(
            directVisits.toHistoryHighlights(),
            visitsFromSearch + directVisits
        )

        assertEquals(
            directVisits.toHistoryHighlightsInternal(),
            result
        )
    }

    @Test
    fun `GIVEN not all highlights have metadata WHEN getHistoryHighlights is called THEN set 0 for the highlights with not found last access time`() {
        val feature = RecentVisitsFeature(mockk(), mockk(), mockk(), mockk(), mockk())
        val visitsFromSearch = getSearchFromHistoryMetadataItems(10)
        val directVisits = getDirectVisitsHistoryMetadataItems(10)
        val highlightsWithUnknownAccessTime = directVisits.toHistoryHighlightsInternal().take(5).map {
            it.copy(lastAccessedTime = 0)
        }
        val highlightsWithInferredAccessTime = directVisits.toHistoryHighlightsInternal().takeLast(5)

        val result = feature.getHistoryHighlights(
            directVisits.toHistoryHighlights(),
            visitsFromSearch + directVisits.takeLast(5)
        )

        assertEquals(
            highlightsWithUnknownAccessTime + highlightsWithInferredAccessTime,
            result
        )
    }

    @Test
    fun `GIVEN multiple metadata records for the same highlight WHEN getHistoryHighlights is called THEN set the latest access time from multiple available`() {
        val feature = RecentVisitsFeature(mockk(), mockk(), mockk(), mockk(), mockk())
        val visitsFromSearch = getSearchFromHistoryMetadataItems(10)
        val directVisits = getDirectVisitsHistoryMetadataItems(10)
        val newerDirectVisits = directVisits.mapIndexed { index, item ->
            item.copy(updatedAt = item.updatedAt * ((index % 2) + 1))
        }

        val result = feature.getHistoryHighlights(
            directVisits.toHistoryHighlights(),
            visitsFromSearch + directVisits + newerDirectVisits
        )

        assertEquals(
            directVisits.mapIndexed { index, item ->
                item.toHistoryHighlightInternal(item.updatedAt * ((index % 2) + 1))
            },
            result
        )
    }

    @Test
    fun `GIVEN highlights don't have a valid title WHEN getCombinedHistory is called THEN the url is set as title`() {
        val feature = RecentVisitsFeature(mockk(), mockk(), mockk(), mockk(), mockk())
        val directVisits = getDirectVisitsHistoryMetadataItems(10).mapIndexed { index, item ->
            when (index % 3) {
                0 -> item
                1 -> item.copy(title = null)
                else -> item.copy(title = " ".repeat(Random.nextInt(3)))
            }
        }
        val sortedByDateHighlights = directVisits.reversed()

        val result = feature.getCombinedHistory(
            directVisits.toHistoryHighlightsInternal(),
        ).filterIsInstance<RecentHistoryHighlight>()

        assertEquals(9, result.size)
        result.forEachIndexed { index, item ->
            when (index % 3) {
                0 -> assertEquals(sortedByDateHighlights[index].title, item.title)
                1 -> assertEquals(sortedByDateHighlights[index].key.url, item.title)
                2 -> assertEquals(sortedByDateHighlights[index].key.url, item.title)
            }
        }
    }

    private fun startRecentVisitsFeature() {
        val feature = RecentVisitsFeature(
            appStore,
            historyMetadataStorage,
            lazy { historyHightlightsStorage },
            scope,
            testDispatcher,
        )

        feature.start()

        scope.advanceUntilIdle()
        appStore.waitUntilIdle()

        coVerify {
            historyMetadataStorage.getHistoryMetadataSince(any())
        }
    }
}

/**
 * Get a list of [HistoryMetadata] representing visits following a search with [count] different elements.
 * The elements will have different `title`, `url`, `searchTerm` and an increasing `updatedAt` property
 * based on their index in the returned list.
 *
 * This items can be mapped to search groups.
 */
private fun getSearchFromHistoryMetadataItems(count: Int): List<HistoryMetadata> {
    return if (count > 0) {
        val historyEntry1 = HistoryMetadata(
            key = HistoryMetadataKey("https://searchurl1.test", "searchTerm1", null),
            title = "test1",
            createdAt = 0,
            updatedAt = 1,
            totalViewTime = 1,
            documentType = DocumentType.Regular,
            previewImageUrl = null
        )
        mutableListOf(historyEntry1) + (2..count).map {
            historyEntry1.copy(
                key = HistoryMetadataKey("https://searchurl$it.test", "searchTerm$it", null),
                title = "test$it",
                updatedAt = it.toLong()
            )
        }
    } else {
        emptyList()
    }
}

/**
 * Get a list of [HistoryMetadata] representing directly accessed webpages with [count] different elements.
 * The elements will have different `title`, `url` and an increasing `updatedAt` property
 * based on their index in the returned list.
 *
 * This items cannot be mapped to search groups since they don't contain a `searchTerm`.
 */
private fun getDirectVisitsHistoryMetadataItems(count: Int): List<HistoryMetadata> {
    return if (count > 0) {
        val historyEntry1 = HistoryMetadata(
            key = HistoryMetadataKey("https://url1.test", null),
            title = "test1",
            createdAt = 0,
            updatedAt = 1,
            totalViewTime = 1,
            documentType = DocumentType.Regular,
            previewImageUrl = null
        )
        mutableListOf(historyEntry1) + (2..count).map {
            historyEntry1.copy(
                key = HistoryMetadataKey("https://url$it.test", null),
                title = "test$it",
                updatedAt = it.toLong()
            )
        }
    } else {
        emptyList()
    }
}

/**
 * Get a list of [HistoryHighlight] with [count] different elements.
 * Each element will have unique value for all properties based on their index in the returned list.
 */
private fun getHistoryHighlightsItems(count: Int): List<HistoryHighlight> =
    (1..count).map {
        HistoryHighlight(
            score = it.toDouble(),
            placeId = it,
            url = "https://url$it.test",
            title = "test$it",
            previewImageUrl = "https://previewImage$it.test"
        )
    }

private fun HistoryMetadata.toHistoryHighlight(): HistoryHighlight = HistoryHighlight(
    score = 3.0,
    placeId = 2,
    title = title,
    url = key.url,
    previewImageUrl = null
)

private fun HistoryMetadata.toRecentHistoryHighlight(): RecentHistoryHighlight =
    RecentHistoryHighlight(
        title = if (title.isNullOrBlank()) key.url else title!!,
        url = key.url
    )

private fun List<HistoryMetadata>.toRecentHistoryHighlights(): List<RecentHistoryHighlight> =
    map { it.toRecentHistoryHighlight() }

@JvmName("historyHighlightsToRecentHistoryHighlights") // avoid platform declaration clash with the above method
private fun List<HistoryHighlight>.toRecentHistoryHighlights(): List<RecentHistoryHighlight> =
    map {
        RecentHistoryHighlight(
            title = it.title!!,
            url = it.url
        )
    }

private fun List<HistoryMetadata>.toHistoryHighlights() = map { it.toHistoryHighlight() }

private fun HistoryMetadata.toHistoryHighlightInternal(lastAccessTime: Long) =
    HistoryHighlightInternal(
        historyHighlight = this.toHistoryHighlight(),
        lastAccessedTime = lastAccessTime
    )

private fun List<HistoryMetadata>.toHistoryHighlightsInternal() = mapIndexed { index, item ->
    item.toHistoryHighlightInternal(index + 1L)
}
