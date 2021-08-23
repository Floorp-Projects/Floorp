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
            visitType = VisitType.LINK
        )
        val visitInfo2 = VisitInfo(
            url = "http://www.firefox.com",
            title = "firefox",
            visitTime = 2,
            visitType = VisitType.LINK
        )
        val visitInfo3 = VisitInfo(
            url = "http://www.wikipedia.com",
            title = "wikipedia",
            visitTime = 1,
            visitType = VisitType.LINK
        )
        val historyEntry1 = HistoryMetadata(
            key = HistoryMetadataKey("http://www.mozilla.com", "mozilla", null),
            title = "mozilla",
            createdAt = 5,
            updatedAt = 5,
            totalViewTime = 10,
            documentType = DocumentType.Regular,
            previewImageUrl = null
        )
        val historyEntry2 = HistoryMetadata(
            key = HistoryMetadataKey("http://www.firefox.com", "mozilla", null),
            title = "firefox",
            createdAt = 2,
            updatedAt = 2,
            totalViewTime = 20,
            documentType = DocumentType.Regular,
            previewImageUrl = null
        )

        coEvery { storage.getVisitsPaginated(any(), any(), any()) } returns listOf(visitInfo1, visitInfo2, visitInfo3)
        coEvery { storage.getHistoryMetadataSince(any()) } returns listOf(historyEntry1, historyEntry2)

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
                items = listOf(
                    History.Metadata(
                        id = historyEntry1.createdAt.toInt(),
                        title = historyEntry1.title!!,
                        url = historyEntry1.key.url,
                        visitedAt = historyEntry1.createdAt,
                        totalViewTime = historyEntry1.totalViewTime
                    ),
                    History.Metadata(
                        id = historyEntry2.createdAt.toInt(),
                        title = historyEntry2.title!!,
                        url = historyEntry2.key.url,
                        visitedAt = historyEntry2.createdAt,
                        totalViewTime = historyEntry2.totalViewTime
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
}
