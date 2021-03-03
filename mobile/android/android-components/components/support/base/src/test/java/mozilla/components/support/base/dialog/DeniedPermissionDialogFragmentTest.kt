/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.dialog

import android.content.DialogInterface.BUTTON_POSITIVE
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.base.R
import mozilla.components.support.test.ext.appCompatContext
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class DeniedPermissionDialogFragmentTest {

    @Test
    fun `WHEN showing the dialog THEN it has the provided message`() {
        val messageId = R.string.mozac_support_base_permissions_needed_negative_button
        val fragment = spy(
            DeniedPermissionDialogFragment.newInstance(messageId)
        )

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)

        dialog.show()

        val messageTextView = dialog.findViewById<TextView>(android.R.id.message)

        assertEquals(fragment.message, messageId)
        assertEquals(messageTextView.text.toString(), testContext.getString(messageId))
    }

    @Test
    fun `WHEN clicking the positive button THEN the settings page will show`() {
        val messageId = R.string.mozac_support_base_permissions_needed_negative_button

        val fragment = spy(
            DeniedPermissionDialogFragment.newInstance(messageId)
        )

        doNothing().`when`(fragment).dismiss()
        doReturn(appCompatContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val positiveButton = (dialog as AlertDialog).getButton(BUTTON_POSITIVE)
        positiveButton.performClick()

        verify(fragment).openSettingsPage()
    }
}
