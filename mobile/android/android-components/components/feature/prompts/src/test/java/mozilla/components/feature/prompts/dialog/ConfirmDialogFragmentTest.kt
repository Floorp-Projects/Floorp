/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.dialog

import android.content.DialogInterface
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.ext.appCompatContext
import org.junit.Assert
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.MockitoAnnotations.initMocks

@RunWith(AndroidJUnit4::class)
class ConfirmDialogFragmentTest {

    @Mock private lateinit var mockFeature: Prompter
    private lateinit var fragment: ConfirmDialogFragment

    @Before
    fun setup() {
        initMocks(this)
        fragment = spy(
            ConfirmDialogFragment.newInstance(
                "sessionId",
                "title",
                "message",
                "positiveLabel",
                "negativeLabel"
            )
        )
    }

    @Test
    fun `build dialog`() {

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

        fragment.feature = mockFeature

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val positiveButton = (dialog as AlertDialog).getButton(DialogInterface.BUTTON_POSITIVE)
        positiveButton.performClick()

        verify(mockFeature).onConfirm("sessionId", null)
    }

    @Test
    fun `clicking on negative button notifies the feature`() {

        fragment.feature = mockFeature

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val negativeButton = (dialog as AlertDialog).getButton(DialogInterface.BUTTON_NEGATIVE)
        negativeButton.performClick()

        verify(mockFeature).onCancel("sessionId")
    }

    @Test
    fun `cancelling the dialog cancels the feature`() {
        doReturn(appCompatContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)

        fragment.feature = mockFeature

        doReturn(appCompatContext).`when`(fragment).requireContext()

        Mockito.doNothing().`when`(fragment).dismiss()

        Assert.assertNotNull(dialog)

        dialog.show()

        fragment.onCancel(dialog)

        verify(mockFeature).onCancel("sessionId")
    }
}
