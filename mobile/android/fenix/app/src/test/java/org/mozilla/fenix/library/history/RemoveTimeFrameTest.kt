/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import org.junit.Assert
import org.junit.Test
import java.util.Calendar
import java.util.concurrent.TimeUnit

class RemoveTimeFrameTest {

    @Test
    fun `WHEN LastHour is calculated THEN first timeStamp is one hour ago`() {
        val lastHourRange = RemoveTimeFrame.LastHour.toLongRange()
        val nowMillis = Calendar.getInstance().timeInMillis
        val millisDif = nowMillis - lastHourRange.first
        val hourDif = TimeUnit.HOURS.convert(millisDif, TimeUnit.MILLISECONDS)
        Assert.assertEquals(1, hourDif)
    }

    @Test
    fun `WHEN LastHour is calculated THEN second timeStamp is equal or greater than now`() {
        val lastHourRange = RemoveTimeFrame.LastHour.toLongRange()
        val nowMillis = Calendar.getInstance().timeInMillis
        Assert.assertTrue(nowMillis <= lastHourRange.last)
    }

    @Test
    fun `WHEN TodayAndYesterday is calculated THEN first timeStamp is one day ago`() {
        val lastHourRange = RemoveTimeFrame.TodayAndYesterday.toLongRange()
        val nowMillis = Calendar.getInstance().timeInMillis
        val millisDif = nowMillis - lastHourRange.first
        val daysDif = TimeUnit.DAYS.convert(millisDif, TimeUnit.MILLISECONDS)
        Assert.assertEquals(1, daysDif)
    }

    @Test
    fun `WHEN TodayAndYesterday is calculated THEN first timeStamp is the start of the previous day`() {
        val todayAndYesterdayRange = RemoveTimeFrame.TodayAndYesterday.toLongRange()
        val yesterdayStartMillis = Calendar.getInstance().apply {
            add(Calendar.DAY_OF_YEAR, -1)
            set(Calendar.HOUR_OF_DAY, 0)
            set(Calendar.MINUTE, 0)
            set(Calendar.SECOND, 0)
            set(Calendar.MILLISECOND, 0)
        }.timeInMillis
        Assert.assertEquals(yesterdayStartMillis, todayAndYesterdayRange.first)
    }

    @Test
    fun `WHEN TodayAndYesterday is calculated THEN second timeStamp is equal or greater than now`() {
        val lastHourRange = RemoveTimeFrame.TodayAndYesterday.toLongRange()
        val nowMillis = Calendar.getInstance().timeInMillis
        Assert.assertTrue(nowMillis <= lastHourRange.last)
    }
}
