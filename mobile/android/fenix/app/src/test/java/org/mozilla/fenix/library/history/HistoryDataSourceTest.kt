/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import mozilla.components.concept.storage.HistoryMetadataKey
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.components.history.HistoryDB

class HistoryDataSourceTest {
    private val testCases = listOf(
        listOf<Int>() to listOf(),

        listOf(1) to listOf(
            TestHistory.Regular("http://www.mozilla.com"),
        ),
        listOf(1, 2) to listOf(
            TestHistory.Regular("http://www.mozilla.com"),
            TestHistory.Regular("http://www.mozilla.com/2"),
        ),
        listOf(1, 2) to listOf(
            TestHistory.Metadata("http://www.mozilla.com"),
            TestHistory.Regular("http://www.mozilla.com/2"),
        ),
        listOf(1, 2, 3) to listOf(
            TestHistory.Metadata("http://www.mozilla.com"),
            TestHistory.Regular("http://www.mozilla.com/2"),
            TestHistory.Metadata("http://www.mozilla.com"),
        ),
        listOf(1, 2, 3, 4, 5) to listOf(
            TestHistory.Metadata("http://www.mozilla.com"),
            TestHistory.Regular("http://www.mozilla.com/2"),
            TestHistory.Metadata("http://www.mozilla.com"),
            TestHistory.Regular("http://www.mozilla.com/3"),
            TestHistory.Regular("http://www.mozilla.com/2"),
        ),
        listOf(1, 2, 3) to listOf(
            TestHistory.Metadata("http://www.mozilla.com"),
            TestHistory.Regular("http://www.mozilla.com/2"),
            TestHistory.Group("firefox", items = listOf()),
        ),
        listOf(1, 2, 3) to listOf(
            TestHistory.Metadata("http://www.mozilla.com"),
            TestHistory.Regular("http://www.mozilla.com/2"),
            TestHistory.Group(
                "firefox",
                items = listOf(
                    "http://www.firefox.com",
                ),
            ),
        ),
        listOf(1, 2, 7) to listOf(
            TestHistory.Metadata("http://www.mozilla.com"),
            TestHistory.Regular("http://www.mozilla.com/2"),
            TestHistory.Group(
                "firefox",
                items = listOf(
                    "http://www.firefox.com",
                    "http://www.firefox.com/2",
                    "http://www.firefox.com/3",
                    "http://www.firefox.com/4",
                    "http://www.firefox.com/5",
                ),
            ),
        ),
        listOf(5, 6, 7) to listOf(
            TestHistory.Group(
                "firefox",
                items = listOf(
                    "http://www.firefox.com",
                    "http://www.firefox.com/2",
                    "http://www.firefox.com/3",
                    "http://www.firefox.com/4",
                    "http://www.firefox.com/5",
                ),
            ),
            TestHistory.Metadata("http://www.mozilla.com"),
            TestHistory.Regular("http://www.mozilla.com/2"),
        ),
        listOf(1, 6, 7) to listOf(
            TestHistory.Metadata("http://www.mozilla.com"),
            TestHistory.Group(
                "firefox",
                items = listOf(
                    "http://www.firefox.com",
                    "http://www.firefox.com/2",
                    "http://www.firefox.com/3",
                    "http://www.firefox.com/4",
                    "http://www.firefox.com/5",
                ),
            ),
            TestHistory.Regular("http://www.mozilla.com/2"),
        ),
        listOf(1, 6, 8, 9) to listOf(
            TestHistory.Metadata("http://www.mozilla.com"),
            TestHistory.Group(
                "firefox",
                items = listOf(
                    "http://www.firefox.com",
                    "http://www.firefox.com/2",
                    "http://www.firefox.com/3",
                    "http://www.firefox.com/4",
                    "http://www.firefox.com/5",
                ),
            ),
            TestHistory.Group(
                "mdn",
                items = listOf(
                    "https://developer.mozilla.org/en-US/1",
                    "https://developer.mozilla.org/en-US/2",
                ),
            ),
            TestHistory.Regular("http://www.mozilla.com/2"),
        ),
        listOf(1) to listOf(
            TestHistory.Group(
                "mozilla",
                items = listOf(
                    "http://www.mozilla.com",
                ),
            ),
        ),
    )

    @Test
    fun `assign positions basics - initial offset`() {
        testCases.forEach {
            verifyPositions(it.first, offset = 0, it.second)
        }
    }

    @Test
    fun `assign position basics - positive offset`() {
        val offset = 25
        testCases.forEach {
            verifyPositions(it.first.map { pos -> pos + offset }, offset = offset, it.second)
        }
    }

    @Test
    fun `assign position basics - negative offset`() {
        // Even though conceptually it doesn't make sense for us to handle negative offsets,
        // as far as simple positioning logic is concerned there's no harm in doing the naive thing.
        // Assertions around offset being a positive value should happen elsewhere, before we're
        // even dealing with positions.
        val offset = -25
        testCases.forEach {
            verifyPositions(it.first.map { pos -> pos + offset }, offset = offset, it.second)
        }
    }

    private fun verifyPositions(expectedPositions: List<Int>, offset: Int, history: List<TestHistory>) {
        assertEquals(
            "For case $history with offset $offset",
            expectedPositions,
            history.toHistoryDB().positionWithOffset(offset).map { it.position },
        )
    }

    private sealed class TestHistory {
        data class Regular(val url: String) : TestHistory()
        data class Metadata(val url: String) : TestHistory()
        data class Group(val title: String, val items: List<String>) : TestHistory()
    }

    // For position tests, we just care about the basic tree structure here,
    // the details (view times, timestamps, etc) don't matter.
    private fun List<TestHistory>.toHistoryDB(): List<HistoryDB> {
        return this.map {
            when (it) {
                is TestHistory.Regular -> {
                    HistoryDB.Regular(
                        title = it.url,
                        url = it.url,
                        visitedAt = 0,
                    )
                }
                is TestHistory.Metadata -> {
                    HistoryDB.Metadata(
                        title = it.url,
                        url = it.url,
                        visitedAt = 0,
                        totalViewTime = 0,
                        historyMetadataKey = HistoryMetadataKey(url = it.url),
                    )
                }
                is TestHistory.Group -> {
                    HistoryDB.Group(
                        title = it.title,
                        visitedAt = 0,
                        items = it.items.map { item ->
                            HistoryDB.Metadata(
                                title = item,
                                url = item,
                                visitedAt = 0,
                                totalViewTime = 0,
                                historyMetadataKey = HistoryMetadataKey(url = item),
                            )
                        },
                    )
                }
            }
        }
    }
}
