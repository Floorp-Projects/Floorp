/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import java.util.Calendar
import java.util.Date

/**
 * A helper class that provides starting and ending timestamps for a set time frame. Is used by
 * [HistoryFragment] to provide timestamps for options inside the delete history dialog.
 */
enum class RemoveTimeFrame {
    LastHour,
    TodayAndYesterday,
    ;

    /**
     * Provides starting and ending timestamps for a set time frame. Each call is calculated at the
     * moment of execution, which is different from [HistoryItemTimeGroup] implementation.
     */
    fun toLongRange(): LongRange {
        return when (this) {
            LastHour -> LongRange(getHourAgo(hoursAgo = 1).time, Long.MAX_VALUE)
            TodayAndYesterday -> LongRange(getDaysAgo(daysAgo = 1).time, Long.MAX_VALUE)
        }
    }

    private fun getHourAgo(hoursAgo: Int): Date {
        return Calendar.getInstance().apply {
            add(Calendar.HOUR_OF_DAY, -hoursAgo)
        }.time
    }

    private fun getDaysAgo(daysAgo: Int): Date {
        return Calendar.getInstance().apply {
            set(Calendar.HOUR_OF_DAY, 0)
            set(Calendar.MINUTE, 0)
            set(Calendar.SECOND, 0)
            set(Calendar.MILLISECOND, 0)
            add(Calendar.DAY_OF_YEAR, -daysAgo)
        }.time
    }
}
