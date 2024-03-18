/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.widget

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.feature.prompts.ext.month
import mozilla.components.feature.prompts.ext.now
import mozilla.components.feature.prompts.ext.toCalendar
import mozilla.components.feature.prompts.ext.year
import mozilla.components.feature.prompts.widget.MonthAndYearPicker.Companion.DEFAULT_MAX_YEAR
import mozilla.components.feature.prompts.widget.MonthAndYearPicker.Companion.DEFAULT_MIN_YEAR
import mozilla.components.support.ktx.kotlin.toDate
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import java.util.Calendar.DECEMBER
import java.util.Calendar.FEBRUARY
import java.util.Calendar.JANUARY

@RunWith(AndroidJUnit4::class)
class MonthAndYearPickerTest {

    @Test
    fun `WHEN picker widget THEN initial values must be displayed`() {
        val initialDate = "2018-06".toDate("yyyy-MM").toCalendar()
        val minDate = "2018-04".toDate("yyyy-MM").toCalendar()
        val maxDate = "2018-09".toDate("yyyy-MM").toCalendar()

        val monthAndYearPicker = MonthAndYearPicker(
            context = testContext,
            selectedDate = initialDate,
            minDate = minDate,
            maxDate = maxDate,
        )

        with(monthAndYearPicker.monthView) {
            assertEquals(initialDate.month, value)
            assertEquals(minDate.month, minValue)
            assertEquals(maxDate.month, maxValue)
        }

        with(monthAndYearPicker.yearView) {
            assertEquals(initialDate.year, value)
            assertEquals(minDate.year, minValue)
            assertEquals(maxDate.year, maxValue)
        }
    }

    @Test
    fun `WHEN selectedDate is a year less than maxDate THEN month picker MUST allow selecting until the last month of the year`() {
        val initialDate = "2018-06".toDate("yyyy-MM").toCalendar()
        val minDate = "2018-04".toDate("yyyy-MM").toCalendar()
        val maxDate = "2019-09".toDate("yyyy-MM").toCalendar()

        val monthAndYearPicker = MonthAndYearPicker(
            context = testContext,
            selectedDate = initialDate,
            minDate = minDate,
            maxDate = maxDate,
        )

        with(monthAndYearPicker.monthView) {
            assertEquals(initialDate.month, value)
            assertEquals(minDate.month, minValue)
            assertEquals(DECEMBER, maxValue)
        }

        with(monthAndYearPicker.yearView) {
            assertEquals(initialDate.year, value)
            assertEquals(minDate.year, minValue)
            assertEquals(maxDate.year, maxValue)
        }
    }

    @Test
    fun `WHEN changing month picker from DEC to JAN THEN year picker MUST be increased by 1`() {
        val initialDate = "2018-06".toDate("yyyy-MM")
        val initialCal = "2018-06".toDate("yyyy-MM").toCalendar()

        val monthAndYearPicker = MonthAndYearPicker(
            context = testContext,
            selectedDate = initialDate.toCalendar(),
        )

        val yearView = monthAndYearPicker.yearView
        assertEquals(initialCal.year, yearView.value)

        monthAndYearPicker.onValueChange(monthAndYearPicker.monthView, DECEMBER, JANUARY)

        assertEquals(initialCal.year + 1, yearView.value)
    }

    @Test
    fun `WHEN changing month picker from JAN to DEC THEN year picker MUST be decreased by 1`() {
        val initialDate = "2018-06".toDate("yyyy-MM")
        val initialCal = "2018-06".toDate("yyyy-MM").toCalendar()

        val monthAndYearPicker = MonthAndYearPicker(
            context = testContext,
            selectedDate = initialDate.toCalendar(),
        )

        val yearView = monthAndYearPicker.yearView
        assertEquals(initialCal.year, yearView.value)

        monthAndYearPicker.onValueChange(monthAndYearPicker.monthView, JANUARY, DECEMBER)

        assertEquals(initialCal.year - 1, yearView.value)
    }

    @Test
    fun `WHEN selecting a month or a year THEN dateSetListener MUST be notified`() {
        val initialDate = "2018-06".toDate("yyyy-MM")
        val initialCal = "2018-06".toDate("yyyy-MM").toCalendar()

        val monthAndYearPicker = MonthAndYearPicker(
            context = testContext,
            selectedDate = initialDate.toCalendar(),
        )

        var newMonth = 0
        var newYear = 0

        monthAndYearPicker.dateSetListener = object : MonthAndYearPicker.OnDateSetListener {
            override fun onDateSet(picker: MonthAndYearPicker, month: Int, year: Int) {
                newMonth = month
                newYear = year
            }
        }

        assertEquals(0, newMonth)
        assertEquals(0, newYear)

        val yearView = monthAndYearPicker.yearView
        val monthView = monthAndYearPicker.monthView

        monthAndYearPicker.onValueChange(yearView, initialCal.year - 1, initialCal.year + 1)

        assertEquals(initialCal.year + 1, newYear)

        monthAndYearPicker.onValueChange(monthView, JANUARY, FEBRUARY)

        assertEquals(FEBRUARY + 1, newMonth) // Month is zero based
    }

    @Test
    fun `WHEN max or min date are in a illogical range THEN picker must allow to select the default values for max and min`() {
        val initialDate = "2018-06".toDate("yyyy-MM").toCalendar()
        val minDate = "2019-04".toDate("yyyy-MM").toCalendar()
        val maxDate = "2018-09".toDate("yyyy-MM").toCalendar()

        val monthAndYearPicker = MonthAndYearPicker(
            context = testContext,
            selectedDate = initialDate,
            minDate = minDate,
            maxDate = maxDate,
        )

        with(monthAndYearPicker.monthView) {
            assertEquals(JANUARY, minValue)
            assertEquals(DECEMBER, maxValue)
        }

        with(monthAndYearPicker.yearView) {
            assertEquals(DEFAULT_MIN_YEAR, minValue)
            assertEquals(DEFAULT_MAX_YEAR, maxValue)
        }
    }

    @Test
    fun `WHEN selecting a date that is before or after min or max date THEN selectDate will be set to min date`() {
        val minDate = "2018-04".toDate("yyyy-MM").toCalendar()
        val maxDate = "2018-09".toDate("yyyy-MM").toCalendar()
        val initialDate = now()

        initialDate.year = minDate.year - 1

        val monthAndYearPicker = MonthAndYearPicker(
            context = testContext,
            selectedDate = initialDate,
            minDate = minDate,
            maxDate = maxDate,
        )

        with(monthAndYearPicker.monthView) {
            assertEquals(minDate.month, value)
        }

        with(monthAndYearPicker.yearView) {
            assertEquals(minDate.year, value)
        }
    }
}
