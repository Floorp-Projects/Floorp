/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import android.content.DialogInterface
import android.content.DialogInterface.BUTTON_POSITIVE
import android.view.ContextThemeWrapper
import android.widget.CheckBox
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import mozilla.components.support.test.mock
import mozilla.components.feature.prompts.R.id
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class AlertDialogFragmentTest {

    private val context: Context
        get() = ContextThemeWrapper(
            ApplicationProvider.getApplicationContext<Context>(),
            androidx.appcompat.R.style.Theme_AppCompat
        )

    @Test
    fun `build dialog`() {

        val fragment = spy(
            AlertDialogFragment.newInstance("sessionId", "title", "message", true)
        )

        doReturn(context).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)

        dialog.show()

        val titleTextView = dialog.findViewById<TextView>(androidx.appcompat.R.id.alertTitle)
        val messageTextView = dialog.findViewById<TextView>(android.R.id.message)
        val checkBox = dialog.findViewById<CheckBox>(id.no_more_dialogs_check_box)

        assertEquals(fragment.sessionId, "sessionId")
        assertEquals(fragment.message, "message")
        assertEquals(fragment.hasShownManyDialogs, true)

        assertEquals(titleTextView.text, "title")
        assertEquals(fragment.title, "title")
        assertEquals(messageTextView.text, "message")
        assertNotNull(checkBox)
    }

    @Test
    fun `Alert with hasShownManyDialogs equals false should not have a checkbox`() {

        val fragment = spy(
            AlertDialogFragment.newInstance("sessionId", "title", "message", false)
        )

        doReturn(context).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)

        dialog.show()

        val checkBox = dialog.findViewById<CheckBox>(id.no_more_dialogs_check_box)

        assertNull(checkBox)
    }

    @Test
    fun `Clicking on positive button notifies the feature`() {

        val mockFeature: PromptFeature = mock()

        val fragment = spy(
            AlertDialogFragment.newInstance("sessionId", "title", "message", false)
        )

        fragment.feature = mockFeature

        doReturn(context).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val positiveButton = (dialog as AlertDialog).getButton(DialogInterface.BUTTON_POSITIVE)
        positiveButton.performClick()

        verify(mockFeature).onCancel("sessionId")
    }

    @Test
    fun `After checking no more dialogs checkbox feature onNoMoreDialogsChecked must be called`() {

        val mockFeature: PromptFeature = mock()

        val fragment = spy(
            AlertDialogFragment.newInstance("sessionId", "title", "message", true)
        )

        fragment.feature = mockFeature

        doReturn(context).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val checkBox = dialog.findViewById<CheckBox>(id.no_more_dialogs_check_box)

        checkBox.isChecked = true

        val positiveButton = (dialog as AlertDialog).getButton(BUTTON_POSITIVE)
        positiveButton.performClick()

        verify(mockFeature).onConfirm("sessionId", true)
    }

    @Test
    fun `touching outside of the dialog must notify the feature onCancel`() {

        val mockFeature: PromptFeature = mock()

        val fragment = spy(
            AlertDialogFragment.newInstance("sessionId", "title", "message", true)
        )

        fragment.feature = mockFeature

        doReturn(context).`when`(fragment).requireContext()

        fragment.onCancel(null)

        verify(mockFeature).onCancel("sessionId")
    }
}
