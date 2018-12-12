/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts

import android.content.Context
import android.content.DialogInterface.BUTTON_NEGATIVE
import android.content.DialogInterface.BUTTON_POSITIVE
import android.content.DialogInterface.BUTTON_NEUTRAL
import android.support.v7.app.AlertDialog
import android.support.v7.appcompat.R
import android.view.ContextThemeWrapper
import android.widget.DatePicker
import android.widget.TextView
import androidx.test.core.app.ApplicationProvider
import mozilla.components.support.ktx.kotlin.toDate
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.robolectric.RobolectricTestRunner
import java.util.Calendar
import java.util.Date

@RunWith(RobolectricTestRunner::class)
class DatePickerDialogFragmentTest {
    private val context: Context
        get() = ContextThemeWrapper(
                ApplicationProvider.getApplicationContext(),
                R.style.Theme_AppCompat
        )

    @Test
    fun `build dialog`() {
        val initialDate = "2019-11-29".toDate("yyyy-MM-dd")
        val minDate = "2019-11-28".toDate("yyyy-MM-dd")
        val maxDate = "2019-11-30".toDate("yyyy-MM-dd")
        val fragment = spy(
                DatePickerDialogFragment.newInstance("sessionId", "title", initialDate, minDate, maxDate)
        )

        doReturn(context).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val titleTextView = dialog.findViewById<TextView>(android.support.v7.appcompat.R.id.alertTitle)
        val datePicker = dialog.findViewById<DatePicker>(mozilla.components.feature.prompts.R.id.date_picker)

        assertEquals(titleTextView.text, "title")
        assertEquals(2019, datePicker.year)
        assertEquals(11, datePicker.month + 1)
        assertEquals(29, datePicker.dayOfMonth)
        assertEquals(minDate, Date(datePicker.minDate))
        assertEquals(maxDate, Date(datePicker.maxDate))

        datePicker.updateDate(2101, 10, 28)

        val selectedDate = fragment.selectedDate
        val calendar = Calendar.getInstance()
        calendar.time = selectedDate

        assertEquals(2101, calendar.year)
        assertEquals(10, calendar.month)
        assertEquals(28, calendar.day)
    }

    @Test
    fun `Clicking on positive, neutral and negative button notifies the feature`() {
        val mockFeature: PromptFeature = mock()
        val initialDate = "2019-11-29".toDate("yyyy-MM-dd")
        val fragment = spy(
                DatePickerDialogFragment.newInstance("sessionId", "", initialDate, null, null)
        )
        fragment.feature = mockFeature

        doReturn(context).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val positiveButton = (dialog as AlertDialog).getButton(BUTTON_POSITIVE)
        positiveButton.performClick()
        verify(mockFeature).onSelect("sessionId", initialDate)

        val neutralButton = dialog.getButton(BUTTON_NEUTRAL)
        neutralButton.performClick()
        verify(mockFeature).onClear("sessionId")

        val negativeButton = dialog.getButton(BUTTON_NEGATIVE)
        negativeButton.performClick()
        verify(mockFeature).onCancel("sessionId")
    }

    @Test
    fun `touching outside of the dialog must notify the feature onCancel`() {
        val mockFeature: PromptFeature = mock()
        val fragment = spy(
                DatePickerDialogFragment.newInstance("sessionId", "", Date(), null, null)
        )
        fragment.feature = mockFeature
        doReturn(context).`when`(fragment).requireContext()
        fragment.onCancel(null)
        verify(mockFeature).onCancel("sessionId")
    }
}