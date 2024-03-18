/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.text.format.DateUtils
import org.junit.Assert.assertEquals
import org.junit.Test
import java.util.Calendar

class HistoryItemTimeGroupTest {

    @Test
    fun `WHEN grouping history item with future date THEN item is grouped to today`() {
        val time = System.currentTimeMillis() + DateUtils.WEEK_IN_MILLIS
        val history = History.Regular(
            position = 1,
            title = "test item",
            url = "url",
            visitedAt = time,
            historyTimeGroup = HistoryItemTimeGroup.timeGroupForTimestamp(time),
        )

        assertEquals(HistoryItemTimeGroup.Today, history.historyTimeGroup)
    }

    @Test
    fun `WHEN grouping history item with today's date THEN item is grouped to today`() {
        val time = System.currentTimeMillis() + DateUtils.MINUTE_IN_MILLIS
        val history = History.Regular(
            position = 1,
            title = "test item",
            url = "url",
            visitedAt = time,
            historyTimeGroup = HistoryItemTimeGroup.timeGroupForTimestamp(time),
        )

        assertEquals(HistoryItemTimeGroup.Today, history.historyTimeGroup)
    }

    @Test
    fun `WHEN grouping history item with today's midnight date THEN item is grouped to today`() {
        val calendar = Calendar.getInstance().apply {
            set(Calendar.HOUR_OF_DAY, 0)
            set(Calendar.MINUTE, 0)
            set(Calendar.SECOND, 0)
        }

        val history = History.Regular(
            position = 1,
            title = "test item",
            url = "url",
            visitedAt = calendar.timeInMillis,
            historyTimeGroup = HistoryItemTimeGroup.timeGroupForTimestamp(calendar.timeInMillis),
        )

        assertEquals(HistoryItemTimeGroup.Today, history.historyTimeGroup)
    }

    @Test
    fun `WHEN grouping history item with yesterday's night date THEN item is grouped to yesterday`() {
        val calendar = Calendar.getInstance().apply {
            set(Calendar.HOUR_OF_DAY, 0)
            set(Calendar.MINUTE, 0)
            set(Calendar.SECOND, 0)
        }

        val time = calendar.timeInMillis - DateUtils.HOUR_IN_MILLIS

        val history = History.Regular(
            position = 1,
            title = "test item",
            url = "url",
            visitedAt = time,
            historyTimeGroup = HistoryItemTimeGroup.timeGroupForTimestamp(time),
        )

        assertEquals(HistoryItemTimeGroup.Yesterday, history.historyTimeGroup)
    }

    @Test
    fun `WHEN grouping history item with 23 hours before midnight date THEN item is grouped to yesterday`() {
        val calendar = Calendar.getInstance()
        calendar.set(Calendar.HOUR_OF_DAY, 0)
        calendar.set(Calendar.MINUTE, 0)
        calendar.set(Calendar.SECOND, 0)

        val time = calendar.timeInMillis - (DateUtils.HOUR_IN_MILLIS * 23)

        val history = History.Regular(
            position = 1,
            title = "test item",
            url = "url",
            visitedAt = time,
            historyTimeGroup = HistoryItemTimeGroup.timeGroupForTimestamp(time),
        )

        assertEquals(HistoryItemTimeGroup.Yesterday, history.historyTimeGroup)
    }

    @Test
    fun `WHEN grouping history item with 25 hours before midnight date THEN item is grouped to this week`() {
        val calendar = Calendar.getInstance().apply {
            set(Calendar.HOUR_OF_DAY, 0)
            set(Calendar.MINUTE, 0)
            set(Calendar.SECOND, 0)
        }

        val time = calendar.timeInMillis - (DateUtils.HOUR_IN_MILLIS * 25)
        val history = History.Regular(
            position = 1,
            title = "test item",
            url = "url",
            visitedAt = time,
            historyTimeGroup = HistoryItemTimeGroup.timeGroupForTimestamp(time),
        )

        assertEquals(HistoryItemTimeGroup.ThisWeek, history.historyTimeGroup)
    }

    @Test
    fun `WHEN grouping history item with 3 days ago date THEN item is grouped to this week`() {
        val time = System.currentTimeMillis() - (DateUtils.DAY_IN_MILLIS * 3)
        val history = History.Regular(
            position = 1,
            title = "test item",
            url = "url",
            visitedAt = time,
            historyTimeGroup = HistoryItemTimeGroup.timeGroupForTimestamp(time),
        )

        assertEquals(HistoryItemTimeGroup.ThisWeek, history.historyTimeGroup)
    }

    @Test
    fun `WHEN grouping history item with 6 days ago date THEN item is grouped to this week`() {
        val time = System.currentTimeMillis() - (DateUtils.DAY_IN_MILLIS * 6)
        val history = History.Regular(
            position = 1,
            title = "test item",
            url = "url",
            visitedAt = time,
            historyTimeGroup = HistoryItemTimeGroup.timeGroupForTimestamp(time),
        )

        assertEquals(HistoryItemTimeGroup.ThisWeek, history.historyTimeGroup)
    }

    @Test
    fun `WHEN grouping history item with 8 days ago date THEN item is grouped to this month`() {
        val time = System.currentTimeMillis() - (DateUtils.DAY_IN_MILLIS * 8)
        val history = History.Regular(
            position = 1,
            title = "test item",
            url = "url",
            visitedAt = time,
            historyTimeGroup = HistoryItemTimeGroup.timeGroupForTimestamp(time),
        )

        assertEquals(HistoryItemTimeGroup.ThisMonth, history.historyTimeGroup)
    }

    @Test
    fun `WHEN grouping history item with 29 days ago date THEN item is grouped to this month`() {
        val time = System.currentTimeMillis() - (DateUtils.DAY_IN_MILLIS * 29)
        val history = History.Regular(
            position = 1,
            title = "test item",
            url = "url",
            visitedAt = time,
            historyTimeGroup = HistoryItemTimeGroup.timeGroupForTimestamp(time),
        )

        assertEquals(HistoryItemTimeGroup.ThisMonth, history.historyTimeGroup)
    }

    @Test
    fun `WHEN grouping history item with 31 days ago date THEN item is grouped to older`() {
        val time = System.currentTimeMillis() - (DateUtils.DAY_IN_MILLIS * 31)
        val history = History.Regular(
            position = 1,
            title = "test item",
            url = "url",
            visitedAt = time,
            historyTimeGroup = HistoryItemTimeGroup.timeGroupForTimestamp(time),
        )

        assertEquals(HistoryItemTimeGroup.Older, history.historyTimeGroup)
    }

    @Test
    fun `WHEN grouping history item with zero date THEN item is grouped to older`() {
        val history = History.Regular(
            position = 1,
            title = "test item",
            url = "url",
            visitedAt = 0,
            historyTimeGroup = HistoryItemTimeGroup.timeGroupForTimestamp(0),
        )

        assertEquals(HistoryItemTimeGroup.Older, history.historyTimeGroup)
    }

    @Test
    fun `WHEN grouping history item with negative date THEN item is grouped to older`() {
        val history = History.Regular(
            position = 1,
            title = "test item",
            url = "url",
            visitedAt = -100,
            historyTimeGroup = HistoryItemTimeGroup.timeGroupForTimestamp(-100),
        )

        assertEquals(HistoryItemTimeGroup.Older, history.historyTimeGroup)
    }
}
