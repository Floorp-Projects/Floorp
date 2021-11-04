/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.history

import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.mockk
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.concept.storage.DocumentType
import mozilla.components.concept.storage.HistoryMetadata
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.concept.storage.VisitInfo
import mozilla.components.concept.storage.VisitType
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.library.history.History

class PagedHistoryProviderTest {

    private lateinit var storage: PlacesHistoryStorage

    @Before
    fun setup() {
        storage = mockk()
    }

    @Test
    fun `getHistory uses getVisitsPaginated`() {
        val provider = DefaultPagedHistoryProvider(
            historyStorage = storage,
            showHistorySearchGroups = true
        )

        val visitInfo1 = VisitInfo(
            url = "http://www.mozilla.com",
            title = "mozilla",
            visitTime = 5,
            visitType = VisitType.LINK,
            previewImageUrl = null
        )
        val visitInfo2 = VisitInfo(
            url = "http://www.firefox.com",
            title = "firefox",
            visitTime = 2,
            visitType = VisitType.LINK,
            previewImageUrl = null
        )
        val visitInfo3 = VisitInfo(
            url = "http://www.wikipedia.com",
            title = "wikipedia",
            visitTime = 1,
            visitType = VisitType.LINK,
            previewImageUrl = null
        )
        val historyMetadataKey1 = HistoryMetadataKey("http://www.mozilla.com", "mozilla", null)
        val historyEntry1 = HistoryMetadata(
            key = historyMetadataKey1,
            title = "mozilla",
            createdAt = 150000000, // a large amount to fall outside of the history page.
            updatedAt = 10,
            totalViewTime = 10,
            documentType = DocumentType.Regular,
            previewImageUrl = null
        )
        val historyMetadataKey2 = HistoryMetadataKey("http://www.firefox.com", "mozilla", null)
        val historyEntry2 = HistoryMetadata(
            key = historyMetadataKey2,
            title = "firefox",
            createdAt = 2,
            updatedAt = 11,
            totalViewTime = 20,
            documentType = DocumentType.Regular,
            previewImageUrl = null
        )

        // Adding a third entry with same url to test de-duping
        val historyMetadataKey3 = HistoryMetadataKey("http://www.firefox.com", "mozilla", null)
        val historyEntry3 = HistoryMetadata(
            key = historyMetadataKey3,
            title = "firefox",
            createdAt = 3,
            updatedAt = 12,
            totalViewTime = 30,
            documentType = DocumentType.Regular,
            previewImageUrl = null
        )

        coEvery { storage.getVisitsPaginated(any(), any(), any()) } returns listOf(visitInfo1, visitInfo2, visitInfo3)
        coEvery { storage.getHistoryMetadataSince(any()) } returns listOf(historyEntry1, historyEntry2, historyEntry3)

        var actualResults: List<History>? = null
        provider.getHistory(10L, 5) {
            actualResults = it
        }

        coVerify {
            storage.getVisitsPaginated(
                offset = 10L,
                count = 5,
                excludeTypes = listOf(
                    VisitType.NOT_A_VISIT,
                    VisitType.DOWNLOAD,
                    VisitType.REDIRECT_TEMPORARY,
                    VisitType.RELOAD,
                    VisitType.EMBED,
                    VisitType.FRAMED_LINK,
                    VisitType.REDIRECT_PERMANENT
                )
            )
        }

        val results = listOf(
            History.Group(
                id = historyEntry1.createdAt.toInt(),
                title = historyEntry1.key.searchTerm!!,
                visitedAt = historyEntry1.createdAt,
                // Results are de-duped by URL and sorted descending by createdAt/visitedAt
                items = listOf(
                    History.Metadata(
                        id = historyEntry1.createdAt.toInt(),
                        title = historyEntry1.title!!,
                        url = historyEntry1.key.url,
                        visitedAt = historyEntry1.createdAt,
                        totalViewTime = historyEntry1.totalViewTime,
                        historyMetadataKey = historyMetadataKey1
                    ),
                    History.Metadata(
                        id = historyEntry3.createdAt.toInt(),
                        title = historyEntry3.title!!,
                        url = historyEntry3.key.url,
                        visitedAt = historyEntry3.createdAt,
                        totalViewTime = historyEntry3.totalViewTime,
                        historyMetadataKey = historyMetadataKey2
                    )
                )
            ),
            History.Regular(
                id = 12,
                title = visitInfo3.title!!,
                url = visitInfo3.url,
                visitedAt = visitInfo3.visitTime
            )
        )
        assertEquals(results, actualResults)
    }

    @Test
    fun `history metadata matching lower bound`() {
        val provider = DefaultPagedHistoryProvider(
            historyStorage = storage,
            showHistorySearchGroups = true
        )
        // Oldest history visit on the page is 15 seconds (buffer time) newer than matching
        // metadata record.
        val visitInfo1 = VisitInfo(
            url = "http://www.mozilla.com",
            title = "mozilla",
            visitTime = 25000,
            visitType = VisitType.LINK,
            previewImageUrl = null
        )

        val historyMetadataKey1 = HistoryMetadataKey("http://www.mozilla.com", "mozilla", null)
        val historyEntry1 = HistoryMetadata(
            key = historyMetadataKey1,
            title = "mozilla",
            createdAt = 10000,
            updatedAt = 10,
            totalViewTime = 10,
            documentType = DocumentType.Regular,
            previewImageUrl = null
        )

        coEvery { storage.getVisitsPaginated(any(), any(), any()) } returns listOf(visitInfo1)
        coEvery { storage.getHistoryMetadataSince(any()) } returns listOf(historyEntry1)

        var actualResults: List<History>? = null
        provider.getHistory(0L, 5) {
            actualResults = it
        }

        coVerify {
            storage.getVisitsPaginated(
                offset = 0L,
                count = 5,
                excludeTypes = listOf(
                    VisitType.NOT_A_VISIT,
                    VisitType.DOWNLOAD,
                    VisitType.REDIRECT_TEMPORARY,
                    VisitType.RELOAD,
                    VisitType.EMBED,
                    VisitType.FRAMED_LINK,
                    VisitType.REDIRECT_PERMANENT
                )
            )
        }

        val results = listOf(
            History.Group(
                id = historyEntry1.createdAt.toInt(),
                title = historyEntry1.key.searchTerm!!,
                visitedAt = historyEntry1.createdAt,
                // Results are de-duped by URL and sorted descending by createdAt/visitedAt
                items = listOf(
                    History.Metadata(
                        id = historyEntry1.createdAt.toInt(),
                        title = historyEntry1.title!!,
                        url = historyEntry1.key.url,
                        visitedAt = historyEntry1.createdAt,
                        totalViewTime = historyEntry1.totalViewTime,
                        historyMetadataKey = historyMetadataKey1
                    )
                )
            )
        )

        assertEquals(results, actualResults)
    }

    @Test
    fun `history metadata matching upper bound`() {
        val provider = DefaultPagedHistoryProvider(
            historyStorage = storage,
            showHistorySearchGroups = true
        )
        // Newest history visit on the page is 15 seconds (buffer time) older than matching
        // metadata record.
        val visitInfo1 = VisitInfo(
            url = "http://www.mozilla.com",
            title = "mozilla",
            visitTime = 10000,
            visitType = VisitType.LINK,
            previewImageUrl = null
        )

        val historyMetadataKey1 = HistoryMetadataKey("http://www.mozilla.com", "mozilla", null)
        val historyEntry1 = HistoryMetadata(
            key = historyMetadataKey1,
            title = "mozilla",
            createdAt = 25000,
            updatedAt = 10,
            totalViewTime = 10,
            documentType = DocumentType.Regular,
            previewImageUrl = null
        )

        coEvery { storage.getVisitsPaginated(any(), any(), any()) } returns listOf(visitInfo1)
        coEvery { storage.getHistoryMetadataSince(any()) } returns listOf(historyEntry1)

        var actualResults: List<History>? = null
        provider.getHistory(0L, 5) {
            actualResults = it
        }

        coVerify {
            storage.getVisitsPaginated(
                offset = 0L,
                count = 5,
                excludeTypes = listOf(
                    VisitType.NOT_A_VISIT,
                    VisitType.DOWNLOAD,
                    VisitType.REDIRECT_TEMPORARY,
                    VisitType.RELOAD,
                    VisitType.EMBED,
                    VisitType.FRAMED_LINK,
                    VisitType.REDIRECT_PERMANENT
                )
            )
        }

        val results = listOf(
            History.Group(
                id = historyEntry1.createdAt.toInt(),
                title = historyEntry1.key.searchTerm!!,
                visitedAt = historyEntry1.createdAt,
                // Results are de-duped by URL and sorted descending by createdAt/visitedAt
                items = listOf(
                    History.Metadata(
                        id = historyEntry1.createdAt.toInt(),
                        title = historyEntry1.title!!,
                        url = historyEntry1.key.url,
                        visitedAt = historyEntry1.createdAt,
                        totalViewTime = historyEntry1.totalViewTime,
                        historyMetadataKey = historyMetadataKey1
                    )
                )
            )
        )

        assertEquals(results, actualResults)
    }
}
