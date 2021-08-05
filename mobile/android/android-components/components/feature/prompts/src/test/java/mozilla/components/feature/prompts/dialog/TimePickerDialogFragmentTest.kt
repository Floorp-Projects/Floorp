/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.dialog

import android.app.AlertDialog
import android.app.DatePickerDialog
import android.app.TimePickerDialog
import android.content.DialogInterface.BUTTON_NEUTRAL
import android.content.DialogInterface.BUTTON_POSITIVE
import android.os.Build.VERSION_CODES.LOLLIPOP
import android.widget.DatePicker
import android.widget.NumberPicker
import android.widget.TimePicker
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.feature.prompts.R
import mozilla.components.feature.prompts.dialog.TimePickerDialogFragment.Companion.SELECTION_TYPE_DATE_AND_TIME
import mozilla.components.feature.prompts.dialog.TimePickerDialogFragment.Companion.SELECTION_TYPE_MONTH
import mozilla.components.feature.prompts.dialog.TimePickerDialogFragment.Companion.SELECTION_TYPE_TIME
import mozilla.components.feature.prompts.ext.month
import mozilla.components.feature.prompts.ext.toCalendar
import mozilla.components.feature.prompts.ext.year
import mozilla.components.support.ktx.kotlin.toDate
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.ext.appCompatContext
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.MockitoAnnotations.openMocks
import org.robolectric.annotation.Config
import java.util.Calendar
import java.util.Date

@RunWith(AndroidJUnit4::class)
class TimePickerDialogFragmentTest {

    @Mock private lateinit var mockFeature: Prompter

    @Before
    fun setup() {
        openMocks(this)
    }

    @Test
    fun `build dialog`() {
        val initialDate = "2019-11-29".toDate("yyyy-MM-dd")
        val minDate = "2019-11-28".toDate("yyyy-MM-dd")
        val maxDate = "2019-11-30".toDate("yyyy-MM-dd")
        val fragment = spy(
            TimePickerDialogFragment.newInstance("sessionId", "uid", true, initialDate, minDate, maxDate)
        )

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val datePicker = (dialog as DatePickerDialog).datePicker
        assertEquals("sessionId", fragment.sessionId)
        assertEquals("uid", fragment.promptRequestUID)
        assertEquals(2019, datePicker.year)
        assertEquals(11, datePicker.month + 1)
        assertEquals(29, datePicker.dayOfMonth)
        assertEquals(minDate, Date(datePicker.minDate))
        assertEquals(maxDate, Date(datePicker.maxDate))
    }

    @Test
    fun `Clicking on positive, neutral and negative button notifies the feature`() {
        val initialDate = "2019-11-29".toDate("yyyy-MM-dd")
        val fragment = spy(
            TimePickerDialogFragment.newInstance("sessionId", "uid", false, initialDate, null, null)
        )
        fragment.feature = mockFeature

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val positiveButton = (dialog as AlertDialog).getButton(BUTTON_POSITIVE)
        positiveButton.performClick()
        verify(mockFeature).onConfirm(eq("sessionId"), eq("uid"), any())

        val neutralButton = dialog.getButton(BUTTON_NEUTRAL)
        neutralButton.performClick()
        verify(mockFeature).onClear("sessionId", "uid")
    }

    @Test
    fun `touching outside of the dialog must notify the feature onCancel`() {
        val fragment = spy(
            TimePickerDialogFragment.newInstance("sessionId", "uid", true, Date(), null, null)
        )
        fragment.feature = mockFeature
        doReturn(testContext).`when`(fragment).requireContext()
        fragment.onCancel(mock())
        verify(mockFeature).onCancel("sessionId", "uid")
    }

    @Test
    fun `onTimeChanged must update the selectedDate`() {

        val dialogPicker = TimePickerDialogFragment.newInstance("sessionId", "uid", false, Date(), null, null)

        dialogPicker.onTimeChanged(mock(), 1, 12)

        val calendar = dialogPicker.selectedDate.toCalendar()

        assertEquals(calendar.hour, 1)
        assertEquals(calendar.minutes, 12)
    }

    @Test
    fun `building a date and time picker`() {
        val initialDate = "2018-06-12T19:30".toDate("yyyy-MM-dd'T'HH:mm")
        val minDate = "2018-06-07T00:00".toDate("yyyy-MM-dd'T'HH:mm")
        val maxDate = "2018-06-14T00:00".toDate("yyyy-MM-dd'T'HH:mm")
        val fragment = spy(
            TimePickerDialogFragment.newInstance(
                "sessionId",
                "uid",
                true,
                initialDate,
                minDate,
                maxDate,
                SELECTION_TYPE_DATE_AND_TIME
            )
        )

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val datePicker = dialog.findViewById<DatePicker>(R.id.date_picker)

        assertEquals(2018, datePicker.year)
        assertEquals(6, datePicker.month + 1)
        assertEquals(12, datePicker.dayOfMonth)

        assertEquals(minDate, Date(datePicker.minDate))
        assertEquals(maxDate, Date(datePicker.maxDate))

        val timePicker = dialog.findViewById<TimePicker>(R.id.datetime_picker)

        assertEquals(19, timePicker.hour)
        assertEquals(30, timePicker.minute)
    }

    @Test
    fun `building a month picker`() {
        val initialDate = "2018-06".toDate("yyyy-MM")
        val minDate = "2018-04".toDate("yyyy-MM")
        val maxDate = "2018-09".toDate("yyyy-MM")

        val initialDateCal = initialDate.toCalendar()
        val minCal = minDate.toCalendar()
        val maxCal = maxDate.toCalendar()

        val fragment = spy(
            TimePickerDialogFragment.newInstance(
                "sessionId",
                "uid",
                false,
                initialDate,
                minDate,
                maxDate,
                SELECTION_TYPE_MONTH
            )
        )

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val monthPicker = dialog.findViewById<NumberPicker>(R.id.month_chooser)
        val yearPicker = dialog.findViewById<NumberPicker>(R.id.year_chooser)

        assertEquals(initialDateCal.year, yearPicker.value)
        assertEquals(initialDateCal.month, monthPicker.value)

        assertEquals(minCal.year, yearPicker.minValue)
        assertEquals(minCal.month, monthPicker.minValue)

        assertEquals(maxCal.year, yearPicker.maxValue)
        assertEquals(maxCal.month, monthPicker.maxValue)

        fragment.onDateSet(mock(), 8, 2019)
        val selectedDate = fragment.selectedDate.toCalendar()

        assertEquals(2019, selectedDate.year)
        assertEquals(7, selectedDate.month)
    }

    @Test
    @Config(sdk = [LOLLIPOP])
    @Suppress("DEPRECATION")
    fun `building a time picker`() {
        val initialDate = "2018-06-12T19:30".toDate("yyyy-MM-dd'T'HH:mm")
        val minDate = "2018-06-07T00:00".toDate("yyyy-MM-dd'T'HH:mm")
        val maxDate = "2018-06-14T00:00".toDate("yyyy-MM-dd'T'HH:mm")
        val fragment = spy(
            TimePickerDialogFragment.newInstance(
                "sessionId",
                "uid",
                true,
                initialDate,
                minDate,
                maxDate,
                SELECTION_TYPE_TIME
            )
        )

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()
        assertTrue(dialog is TimePickerDialog)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `creating a TimePickerDialogFragment with an invalid type selection will throw an exception`() {
        val initialDate = "2018-06-12T19:30".toDate("yyyy-MM-dd'T'HH:mm")
        val minDate = "2018-06-07T00:00".toDate("yyyy-MM-dd'T'HH:mm")
        val maxDate = "2018-06-14T00:00".toDate("yyyy-MM-dd'T'HH:mm")
        val fragment = spy(
            TimePickerDialogFragment.newInstance(
                "sessionId",
                "uid",
                false,
                initialDate,
                minDate,
                maxDate,
                -223
            )
        )

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()
    }

    @Test(expected = IllegalArgumentException::class)
    fun `creating a TimePickerDialogFragment with empty title and an invalid type selection will throw an exception `() {
        val initialDate = "2018-06-12T19:30".toDate("yyyy-MM-dd'T'HH:mm")
        val minDate = "2018-06-07T00:00".toDate("yyyy-MM-dd'T'HH:mm")
        val maxDate = "2018-06-14T00:00".toDate("yyyy-MM-dd'T'HH:mm")
        val fragment = spy(
            TimePickerDialogFragment.newInstance(
                "sessionId",
                "uid",
                true,
                initialDate,
                minDate,
                maxDate,
                -223
            )
        )

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()
    }

    private val Calendar.minutes: Int
        get() = get(Calendar.MINUTE)
    private val Calendar.hour: Int
        get() = get(Calendar.HOUR_OF_DAY)
}
