/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts

import android.content.DialogInterface
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.ext.appCompatContext
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class ConfirmDialogFragmentTest {

    @Test
    fun `build dialog`() {

        val fragment = spy(
            ConfirmDialogFragment.newInstance(
                "sessionId",
                "title",
                "message",
                "positiveLabel",
                "negativeLabel"
            )
        )

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)

        dialog.show()

        val titleTextView = dialog.findViewById<TextView>(androidx.appcompat.R.id.alertTitle)
        val messageTextView = dialog.findViewById<TextView>(android.R.id.message)

        assertEquals(fragment.sessionId, "sessionId")
        assertEquals(fragment.message, "message")

        val positiveButton = (dialog as AlertDialog).getButton(DialogInterface.BUTTON_POSITIVE)
        val negativeButton = dialog.getButton(DialogInterface.BUTTON_NEGATIVE)

        assertEquals(titleTextView.text, "title")
        assertEquals(messageTextView.text, "message")
        assertEquals(positiveButton.text, "positiveLabel")
        assertEquals(negativeButton.text, "negativeLabel")
    }

    @Test
    fun `clicking on positive button notifies the feature`() {

        val mockFeature: PromptFeature = mock()

        val fragment = spy(
            ConfirmDialogFragment.newInstance(
                "sessionId",
                "title",
                "message",
                "positiveLabel",
                "negativeLabel"
            )
        )

        fragment.feature = mockFeature

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val positiveButton = (dialog as AlertDialog).getButton(DialogInterface.BUTTON_POSITIVE)
        positiveButton.performClick()

        verify(mockFeature).onConfirm("sessionId")
    }

    @Test
    fun `clicking on negative button notifies the feature`() {

        val mockFeature: PromptFeature = mock()

        val fragment = spy(
            ConfirmDialogFragment.newInstance(
                "sessionId",
                "title",
                "message",
                "positiveLabel",
                "negativeLabel"
            )
        )

        fragment.feature = mockFeature

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val negativeButton = (dialog as AlertDialog).getButton(DialogInterface.BUTTON_NEGATIVE)
        negativeButton.performClick()

        verify(mockFeature).onCancel("sessionId")
    }
}
