/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.widget

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.feature.prompts.ext.hour
import mozilla.components.feature.prompts.ext.millisecond
import mozilla.components.feature.prompts.ext.minute
import mozilla.components.feature.prompts.ext.now
import mozilla.components.feature.prompts.ext.second
import mozilla.components.feature.prompts.ext.toCalendar
import mozilla.components.support.ktx.kotlin.toDate
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class TimePrecisionPickerTest {
    private val initialTime = "12:00".toDate("HH:mm").toCalendar()
    private var minTime = "10:30".toDate("HH:mm").toCalendar()
    private var maxTime = "18:45".toDate("HH:mm").toCalendar()
    private val stepValue = 0.1f

    @Test
    fun `WHEN picker widget THEN initial values must be displayed`() {
        val timePicker = TimePrecisionPicker(
            context = testContext,
            selectedTime = initialTime,
            stepValue = stepValue,
        )

        assertEquals(initialTime.hour, timePicker.hourView.value)
        assertEquals(initialTime.minute, timePicker.minuteView.value)
    }

    @Test
    fun `WHEN selectedTime is outside the bounds of min and max time THEN the displayed time is minTime`() {
        minTime = "14:30".toDate("HH:mm").toCalendar()
        val timePicker = TimePrecisionPicker(
            context = testContext,
            selectedTime = initialTime,
            minTime = minTime,
            maxTime = maxTime,
            stepValue = stepValue,
        )

        assertEquals(minTime.hour, timePicker.hourView.value)
        assertEquals(minTime.minute, timePicker.minuteView.value)
    }

    @Test
    fun `WHEN minTime and maxTime are in illogical order AND selectedTime is outside limits THEN the min and max limits are ignored when initializing the time`() {
        minTime = "15:30".toDate("HH:mm").toCalendar()
        maxTime = "09:30".toDate("HH:mm").toCalendar()
        val timePicker = TimePrecisionPicker(
            context = testContext,
            selectedTime = initialTime,
            minTime = minTime,
            maxTime = maxTime,
            stepValue = stepValue,
        )

        assertEquals(initialTime.hour, timePicker.hourView.value)
        assertEquals(initialTime.minute, timePicker.minuteView.value)
    }

    @Test
    fun `WHEN changing the selected time THEN timeSetListener MUST be notified`() {
        val updatedTime = now()

        val timePicker = TimePrecisionPicker(
            context = testContext,
            selectedTime = initialTime,
            minTime = minTime,
            maxTime = maxTime,
            stepValue = stepValue,
            timeSetListener = object : TimePrecisionPicker.OnTimeSetListener {
                override fun onTimeSet(
                    picker: TimePrecisionPicker,
                    hour: Int,
                    minute: Int,
                    second: Int,
                    millisecond: Int,
                ) {
                    updatedTime.hour = hour
                    updatedTime.minute = minute
                    updatedTime.second = second
                    updatedTime.millisecond = millisecond
                }
            },
        )

        timePicker.onValueChange(timePicker.hourView, initialTime.hour, 13)
        timePicker.onValueChange(timePicker.minuteView, initialTime.minute, 20)
        timePicker.onValueChange(timePicker.secondView, initialTime.second, 30)
        timePicker.onValueChange(timePicker.millisecondView, initialTime.millisecond, 100)

        assertEquals(13, updatedTime.hour)
        assertEquals(20, updatedTime.minute)
        assertEquals(30, updatedTime.second)
        assertEquals(100, updatedTime.millisecond)
    }
}
