/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.content.Context
import org.mozilla.fenix.R
import java.util.Calendar
import java.util.Date

enum class HistoryItemTimeGroup {
    Today, Yesterday, ThisWeek, ThisMonth, Older;

    fun humanReadable(context: Context): String = when (this) {
        Today -> context.getString(R.string.history_today)
        Yesterday -> context.getString(R.string.history_yesterday)
        ThisWeek -> context.getString(R.string.history_7_days)
        ThisMonth -> context.getString(R.string.history_30_days)
        Older -> context.getString(R.string.history_older)
    }

    companion object {
        private const val zeroDays = 0
        private const val oneDay = 1
        private const val sevenDays = 7
        private const val thirtyDays = 30
        private val today = getDaysAgo(zeroDays).time
        private val yesterday = getDaysAgo(oneDay).time
        private val sevenDaysAgo = getDaysAgo(sevenDays).time
        private val thirtyDaysAgo = getDaysAgo(thirtyDays).time
        private val todayRange = LongRange(today, Long.MAX_VALUE) // all future time is considered today
        private val yesterdayRange = LongRange(yesterday, today)
        private val lastWeekRange = LongRange(sevenDaysAgo, yesterday)
        private val lastMonthRange = LongRange(thirtyDaysAgo, sevenDaysAgo)

        private fun getDaysAgo(daysAgo: Int): Date {
            return Calendar.getInstance().apply {
                set(Calendar.HOUR_OF_DAY, 0)
                set(Calendar.MINUTE, 0)
                set(Calendar.SECOND, 0)
                set(Calendar.MILLISECOND, 0)
                add(Calendar.DAY_OF_YEAR, -daysAgo)
            }.time
        }

        internal fun timeGroupForTimestamp(timestamp: Long): HistoryItemTimeGroup {
            return when {
                todayRange.contains(timestamp) -> Today
                yesterdayRange.contains(timestamp) -> Yesterday
                lastWeekRange.contains(timestamp) -> ThisWeek
                lastMonthRange.contains(timestamp) -> ThisMonth
                else -> Older
            }
        }
    }
}
