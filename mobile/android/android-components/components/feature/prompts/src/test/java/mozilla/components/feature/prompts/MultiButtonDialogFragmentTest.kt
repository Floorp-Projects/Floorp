/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts

import android.content.DialogInterface
import android.content.DialogInterface.BUTTON_POSITIVE
import android.widget.CheckBox
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.ext.appCompatContext
import mozilla.components.feature.prompts.R.id
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class MultiButtonDialogFragmentTest {

    @Test
    fun `Build dialog`() {

        val fragment = spy(
            MultiButtonDialogFragment.newInstance(
                "sessionId",
                "title",
                "message",
                true,
                "positiveButton",
                "negativeButton",
                "neutralButton"
            )
        )

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)

        dialog.show()

        val titleTextView = dialog.findViewById<TextView>(androidx.appcompat.R.id.alertTitle)
        val messageTextView = dialog.findViewById<TextView>(android.R.id.message)
        val checkBox = dialog.findViewById<CheckBox>(id.no_more_dialogs_check_box)
        val positiveButton = (dialog as AlertDialog).getButton(BUTTON_POSITIVE)
        val negativeButton = (dialog).getButton(DialogInterface.BUTTON_NEGATIVE)
        val neutralButton = (dialog).getButton(DialogInterface.BUTTON_NEUTRAL)

        assertEquals(fragment.sessionId, "sessionId")
        assertEquals(fragment.message, "message")
        assertEquals(fragment.hasShownManyDialogs, true)

        assertEquals(titleTextView.text, "title")
        assertEquals(fragment.title, "title")
        assertEquals(messageTextView.text, "message")
        assertNotNull(checkBox)

        assertEquals(positiveButton.text, "positiveButton")
        assertEquals(negativeButton.text, "negativeButton")
        assertEquals(neutralButton.text, "neutralButton")
    }

    @Test
    fun `Dialog with hasShownManyDialogs equals false should not have a checkbox`() {

        val fragment = spy(
            MultiButtonDialogFragment.newInstance(
                "sessionId",
                "title",
                "message",
                false,
                "positiveButton",
                "negativeButton",
                "neutralButton"
            )
        )

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)

        dialog.show()

        val checkBox = dialog.findViewById<CheckBox>(id.no_more_dialogs_check_box)

        assertNull(checkBox)
    }

    @Test
    fun `Clicking on a positive button notifies the feature`() {

        val mockFeature: PromptFeature = mock()

        val fragment = spy(
            MultiButtonDialogFragment.newInstance(
                "sessionId",
                "title",
                "message",
                false,
                "positiveButton"
            )
        )

        fragment.feature = mockFeature

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val positiveButton = (dialog as AlertDialog).getButton(BUTTON_POSITIVE)
        positiveButton.performClick()

        verify(mockFeature).onConfirm("sessionId", false to MultiButtonDialogFragment.ButtonType.POSITIVE)
    }

    @Test
    fun `Clicking on a negative button notifies the feature`() {

        val mockFeature: PromptFeature = mock()

        val fragment = spy(
            MultiButtonDialogFragment.newInstance(
                "sessionId",
                "title",
                "message",
                false,
                negativeButton = "negative"
            )
        )

        fragment.feature = mockFeature

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val negativeButton = (dialog as AlertDialog).getButton(DialogInterface.BUTTON_NEGATIVE)
        negativeButton.performClick()

        verify(mockFeature).onConfirm("sessionId", false to MultiButtonDialogFragment.ButtonType.NEGATIVE)
    }

    @Test
    fun `Clicking on a neutral button notifies the feature`() {

        val mockFeature: PromptFeature = mock()

        val fragment = spy(
            MultiButtonDialogFragment.newInstance(
                "sessionId",
                "title",
                "message",
                false,
                neutralButton = "neutral"
            )
        )

        fragment.feature = mockFeature

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val neutralButton = (dialog as AlertDialog).getButton(DialogInterface.BUTTON_NEUTRAL)
        neutralButton.performClick()

        verify(mockFeature).onConfirm("sessionId", false to MultiButtonDialogFragment.ButtonType.NEUTRAL)
    }

    @Test
    fun `After checking no more dialogs checkbox onConfirm must be called with NoMoreDialogs equals true`() {

        val mockFeature: PromptFeature = mock()

        val fragment = spy(
            MultiButtonDialogFragment.newInstance(
                "sessionId",
                "title",
                "message",
                true,
                positiveButton = "positive"
            )
        )

        fragment.feature = mockFeature

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val checkBox = dialog.findViewById<CheckBox>(id.no_more_dialogs_check_box)

        checkBox.isChecked = true

        val positiveButton = (dialog as AlertDialog).getButton(BUTTON_POSITIVE)
        positiveButton.performClick()

        verify(mockFeature).onConfirm("sessionId", true to MultiButtonDialogFragment.ButtonType.POSITIVE)
    }

    @Test
    fun `Touching outside of the dialog must notify the feature onCancel`() {

        val mockFeature: PromptFeature = mock()

        val fragment = spy(
            MultiButtonDialogFragment.newInstance(
                "sessionId",
                "title",
                "message",
                true,
                positiveButton = "positive"
            )
        )

        fragment.feature = mockFeature

        doReturn(appCompatContext).`when`(fragment).requireContext()

        fragment.onCancel(mock())

        verify(mockFeature).onCancel("sessionId")
    }
}
