/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.history

import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.mockk
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.concept.storage.DocumentType
import mozilla.components.concept.storage.HistoryMetadata
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.concept.storage.VisitInfo
import mozilla.components.concept.storage.VisitType
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.utils.Settings

class PagedHistoryProviderTest {

    private lateinit var storage: PlacesHistoryStorage

    @Before
    fun setup() {
        storage = mockk()
        Settings.SEARCH_GROUP_MINIMUM_SITES = 1
    }

    @Test
    fun `getHistory uses getVisitsPaginated`() = runTest {
        val provider = DefaultPagedHistoryProvider(
            historyStorage = storage,
        )

        val visitInfo1 = VisitInfo(
            url = "http://www.mozilla.com",
            title = "mozilla",
            visitTime = 5,
            visitType = VisitType.LINK,
            previewImageUrl = null,
            isRemote = false,
        )
        val visitInfo2 = VisitInfo(
            url = "http://www.firefox.com",
            title = "firefox",
            visitTime = 2,
            visitType = VisitType.LINK,
            previewImageUrl = null,
            isRemote = false,
        )
        val visitInfo3 = VisitInfo(
            url = "http://www.wikipedia.com",
            title = "wikipedia",
            visitTime = 1,
            visitType = VisitType.LINK,
            previewImageUrl = null,
            isRemote = false,
        )
        val historyMetadataKey1 = HistoryMetadataKey("http://www.mozilla.com", "mozilla", null)
        val historyEntry1 = HistoryMetadata(
            key = historyMetadataKey1,
            title = "mozilla",
            createdAt = 150000000, // a large amount to fall outside of the history page.
            updatedAt = 10,
            totalViewTime = 10,
            documentType = DocumentType.Regular,
            previewImageUrl = null,
        )
        val historyMetadataKey2 = HistoryMetadataKey("http://www.firefox.com", "mozilla", null)
        val historyEntry2 = HistoryMetadata(
            key = historyMetadataKey2,
            title = "firefox",
            createdAt = 2,
            updatedAt = 11,
            totalViewTime = 20,
            documentType = DocumentType.Regular,
            previewImageUrl = null,
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
            previewImageUrl = null,
        )

        coEvery { storage.getVisitsPaginated(any(), any(), any()) } returns listOf(visitInfo1, visitInfo2, visitInfo3)
        coEvery { storage.getDetailedVisits(any(), any(), any()) } returns emptyList()
        coEvery { storage.getHistoryMetadataSince(any()) } returns listOf(historyEntry1, historyEntry2, historyEntry3)

        val actualResults: List<HistoryDB> = provider.getHistory(10, 5)

        coVerify {
            storage.getVisitsPaginated(
                offset = 10L,
                count = 5,
                excludeTypes = listOf(
                    VisitType.DOWNLOAD,
                    VisitType.REDIRECT_PERMANENT,
                    VisitType.REDIRECT_TEMPORARY,
                    VisitType.RELOAD,
                    VisitType.EMBED,
                    VisitType.FRAMED_LINK,
                ),
            )
        }

        val results = listOf(
            HistoryDB.Group(
                title = historyEntry1.key.searchTerm!!,
                visitedAt = historyEntry1.createdAt,
                // Results are de-duped by URL and sorted descending by createdAt/visitedAt
                items = listOf(
                    HistoryDB.Metadata(
                        title = historyEntry1.title!!,
                        url = historyEntry1.key.url,
                        visitedAt = historyEntry1.createdAt,
                        totalViewTime = historyEntry1.totalViewTime,
                        historyMetadataKey = historyMetadataKey1,
                    ),
                    HistoryDB.Metadata(
                        title = historyEntry3.title!!,
                        url = historyEntry3.key.url,
                        visitedAt = historyEntry3.createdAt,
                        totalViewTime = historyEntry3.totalViewTime,
                        historyMetadataKey = historyMetadataKey2,
                    ),
                ),
            ),
            HistoryDB.Regular(
                title = visitInfo3.title!!,
                url = visitInfo3.url,
                visitedAt = visitInfo3.visitTime,
            ),
        )
        assertEquals(results, actualResults)
    }

    @Test
    fun `history metadata matching lower bound`() = runTest {
        val provider = DefaultPagedHistoryProvider(
            historyStorage = storage,
        )
        // Oldest history visit on the page is 15 seconds (buffer time) newer than matching
        // metadata record.
        val visitInfo1 = VisitInfo(
            url = "http://www.mozilla.com",
            title = "mozilla",
            visitTime = 25000,
            visitType = VisitType.LINK,
            previewImageUrl = null,
            isRemote = false,
        )

        val historyMetadataKey1 = HistoryMetadataKey("http://www.mozilla.com", "mozilla", null)
        val historyEntry1 = HistoryMetadata(
            key = historyMetadataKey1,
            title = "mozilla",
            createdAt = 10000,
            updatedAt = 10,
            totalViewTime = 10,
            documentType = DocumentType.Regular,
            previewImageUrl = null,
        )

        coEvery { storage.getVisitsPaginated(any(), any(), any()) } returns listOf(visitInfo1)
        coEvery { storage.getDetailedVisits(any(), any(), any()) } returns emptyList()
        coEvery { storage.getHistoryMetadataSince(any()) } returns listOf(historyEntry1)

        val actualResults: List<HistoryDB> = provider.getHistory(0, 5)

        coVerify {
            storage.getVisitsPaginated(
                offset = 0L,
                count = 5,
                excludeTypes = listOf(
                    VisitType.DOWNLOAD,
                    VisitType.REDIRECT_PERMANENT,
                    VisitType.REDIRECT_TEMPORARY,
                    VisitType.RELOAD,
                    VisitType.EMBED,
                    VisitType.FRAMED_LINK,
                ),
            )
        }

        val results = listOf(
            HistoryDB.Group(
                title = historyEntry1.key.searchTerm!!,
                visitedAt = historyEntry1.createdAt,
                // Results are de-duped by URL and sorted descending by createdAt/visitedAt
                items = listOf(
                    HistoryDB.Metadata(
                        title = historyEntry1.title!!,
                        url = historyEntry1.key.url,
                        visitedAt = historyEntry1.createdAt,
                        totalViewTime = historyEntry1.totalViewTime,
                        historyMetadataKey = historyMetadataKey1,
                    ),
                ),
            ),
        )

        assertEquals(results, actualResults)
    }

    @Test
    fun `history metadata matching upper bound`() = runTest {
        val provider = DefaultPagedHistoryProvider(
            historyStorage = storage,
        )
        // Newest history visit on the page is 15 seconds (buffer time) older than matching
        // metadata record.
        val visitInfo1 = VisitInfo(
            url = "http://www.mozilla.com",
            title = "mozilla",
            visitTime = 10000,
            visitType = VisitType.LINK,
            previewImageUrl = null,
            isRemote = false,
        )

        val historyMetadataKey1 = HistoryMetadataKey("http://www.mozilla.com", "mozilla", null)
        val historyEntry1 = HistoryMetadata(
            key = historyMetadataKey1,
            title = "mozilla",
            createdAt = 25000,
            updatedAt = 10,
            totalViewTime = 10,
            documentType = DocumentType.Regular,
            previewImageUrl = null,
        )

        coEvery { storage.getVisitsPaginated(any(), any(), any()) } returns listOf(visitInfo1)
        coEvery { storage.getDetailedVisits(any(), any(), any()) } returns emptyList()
        coEvery { storage.getHistoryMetadataSince(any()) } returns listOf(historyEntry1)

        val actualResults: List<HistoryDB> = provider.getHistory(0, 5)

        coVerify {
            storage.getVisitsPaginated(
                offset = 0L,
                count = 5,
                excludeTypes = listOf(
                    VisitType.DOWNLOAD,
                    VisitType.REDIRECT_PERMANENT,
                    VisitType.REDIRECT_TEMPORARY,
                    VisitType.RELOAD,
                    VisitType.EMBED,
                    VisitType.FRAMED_LINK,
                ),
            )
        }

        val results = listOf(
            HistoryDB.Group(
                title = historyEntry1.key.searchTerm!!,
                visitedAt = historyEntry1.createdAt,
                // Results are de-duped by URL and sorted descending by createdAt/visitedAt
                items = listOf(
                    HistoryDB.Metadata(
                        title = historyEntry1.title!!,
                        url = historyEntry1.key.url,
                        visitedAt = historyEntry1.createdAt,
                        totalViewTime = historyEntry1.totalViewTime,
                        historyMetadataKey = historyMetadataKey1,
                    ),
                ),
            ),
        )

        assertEquals(results, actualResults)
    }

    @Test
    fun `redirects are filtered out from history metadata groups`() = runTest {
        val provider = DefaultPagedHistoryProvider(
            historyStorage = storage,
        )

        val visitInfo1 = VisitInfo(
            url = "http://www.mozilla.com",
            title = "mozilla",
            visitTime = 5,
            visitType = VisitType.LINK,
            previewImageUrl = null,
            isRemote = false,
        )
        val visitInfo2 = VisitInfo(
            url = "http://www.firefox.com",
            title = "firefox",
            visitTime = 2,
            visitType = VisitType.LINK,
            previewImageUrl = null,
            isRemote = false,
        )
        val visitInfo3 = VisitInfo(
            url = "http://www.google.com/link?url=http://www.firefox.com",
            title = "",
            visitTime = 1,
            visitType = VisitType.REDIRECT_TEMPORARY,
            previewImageUrl = null,
            isRemote = false,
        )
        val visitInfo4 = VisitInfo(
            url = "http://mozilla.com",
            title = "",
            visitTime = 1,
            visitType = VisitType.REDIRECT_PERMANENT,
            previewImageUrl = null,
            isRemote = false,
        )

        val historyMetadataKey1 = HistoryMetadataKey("http://www.mozilla.com", "mozilla", null)
        val historyEntry1 = HistoryMetadata(
            key = historyMetadataKey1,
            title = "mozilla",
            createdAt = 1,
            updatedAt = 10,
            totalViewTime = 10,
            documentType = DocumentType.Regular,
            previewImageUrl = null,
        )
        val historyMetadataKey2 = HistoryMetadataKey("http://www.firefox.com", "mozilla", null)
        val historyEntry2 = HistoryMetadata(
            key = historyMetadataKey2,
            title = "firefox",
            createdAt = 2,
            updatedAt = 11,
            totalViewTime = 20,
            documentType = DocumentType.Regular,
            previewImageUrl = null,
        )
        val historyMetadataKey3 = HistoryMetadataKey("http://www.google.com/link?url=http://www.firefox.com", "mozilla", null)
        val historyEntry3 = HistoryMetadata(
            key = historyMetadataKey3,
            title = "",
            createdAt = 2,
            updatedAt = 11,
            totalViewTime = 0,
            documentType = DocumentType.Regular,
            previewImageUrl = null,
        )
        val historyMetadataKey4 = HistoryMetadataKey("http://mozilla.com", "mozilla", null)
        val historyEntry4 = HistoryMetadata(
            key = historyMetadataKey4,
            title = "",
            createdAt = 2,
            updatedAt = 11,
            totalViewTime = 0,
            documentType = DocumentType.Regular,
            previewImageUrl = null,
        )

        // Normal visits.
        coEvery {
            storage.getVisitsPaginated(
                any(),
                any(),
                eq(
                    listOf(
                        VisitType.DOWNLOAD,
                        VisitType.REDIRECT_PERMANENT,
                        VisitType.REDIRECT_TEMPORARY,
                        VisitType.RELOAD,
                        VisitType.EMBED,
                        VisitType.FRAMED_LINK,
                    ),
                ),
            )
        } returns listOf(visitInfo1, visitInfo2)
        // Redirects.
        coEvery {
            storage.getDetailedVisits(
                any(),
                any(),
                eq(
                    VisitType.values().filterNot {
                        it == VisitType.REDIRECT_PERMANENT || it == VisitType.REDIRECT_TEMPORARY
                    },
                ),
            )
        } returns listOf(visitInfo3, visitInfo4)

        coEvery { storage.getHistoryMetadataSince(any()) } returns listOf(historyEntry1, historyEntry2, historyEntry3, historyEntry4)

        val actualResults: List<HistoryDB> = provider.getHistory(10, 5)

        coVerify {
            storage.getVisitsPaginated(
                offset = 10L,
                count = 5,
                excludeTypes = listOf(
                    VisitType.DOWNLOAD,
                    VisitType.REDIRECT_PERMANENT,
                    VisitType.REDIRECT_TEMPORARY,
                    VisitType.RELOAD,
                    VisitType.EMBED,
                    VisitType.FRAMED_LINK,
                ),
            )
        }

        val results = listOf(
            HistoryDB.Group(
                title = historyEntry2.key.searchTerm!!,
                visitedAt = historyEntry2.createdAt,
                items = listOf(
                    HistoryDB.Metadata(
                        title = historyEntry2.title!!,
                        url = historyEntry2.key.url,
                        visitedAt = historyEntry2.createdAt,
                        totalViewTime = historyEntry2.totalViewTime,
                        historyMetadataKey = historyMetadataKey2,
                    ),
                    HistoryDB.Metadata(
                        title = historyEntry1.title!!,
                        url = historyEntry1.key.url,
                        visitedAt = historyEntry1.createdAt,
                        totalViewTime = historyEntry1.totalViewTime,
                        historyMetadataKey = historyMetadataKey1,
                    ),
                ),
            ),
        )
        assertEquals(results, actualResults)
    }

    @Test
    fun `WHEN removeConsecutiveDuplicates is called THEN all consecutive duplicates must be removed`() {
        val results = listOf(
            HistoryDB.Group(
                title = "Group 1",
                visitedAt = 0,
                items = emptyList(),
            ),
            HistoryDB.Regular(
                title = "No duplicate item",
                url = "url",
                visitedAt = 0,
            ),
            HistoryDB.Regular(
                title = "Duplicate item 1",
                url = "url",
                visitedAt = 0,
            ),
            HistoryDB.Regular(
                title = "Duplicate item 2",
                url = "url",
                visitedAt = 0,
            ),
            HistoryDB.Group(
                title = "Group 5",
                visitedAt = 0,
                items = emptyList(),
            ),
            HistoryDB.Regular(
                title = "No duplicate item",
                url = "url",
                visitedAt = 0,
            ),
        ).removeConsecutiveDuplicates()

        val expectedList = listOf(
            HistoryDB.Group(
                title = "Group 1",
                visitedAt = 0,
                items = emptyList(),
            ),
            HistoryDB.Regular(
                title = "No duplicate item",
                url = "url",
                visitedAt = 0,
            ),
            HistoryDB.Group(
                title = "Group 5",
                visitedAt = 0,
                items = emptyList(),
            ),
            HistoryDB.Regular(
                title = "No duplicate item",
                url = "url",
                visitedAt = 0,
            ),
        )
        assertEquals(expectedList, results)
    }
}
