/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.app.links

import android.os.Looper.getMainLooper
import android.widget.Button
import androidx.fragment.app.FragmentManager
import androidx.fragment.app.FragmentTransaction
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.robolectric.Shadows.shadowOf

@RunWith(AndroidJUnit4::class)
class SimpleRedirectDialogFragmentTest {
    private val webUrl = "https://example.com"

    @Test
    fun `Dialog confirmed callback is called correctly`() {
        var onConfirmCalled = false
        var onCancelCalled = false

        val onConfirm = { onConfirmCalled = true }
        val onCancel = { onCancelCalled = true }

        val fragment = spy(SimpleRedirectDialogFragment.newInstance(themeResId = R.style.Theme_AppCompat_Light))
        doNothing().`when`(fragment).dismiss()

        doReturn(testContext).`when`(fragment).requireContext()
        @Suppress("DEPRECATION")
        doReturn(mockFragmentManager()).`when`(fragment).requireFragmentManager()

        fragment.setAppLinkRedirectUrl(webUrl)
        fragment.onConfirmRedirect = onConfirm
        fragment.onCancelRedirect = onCancel

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val confirmButton = dialog.findViewById<Button>(android.R.id.button1)
        confirmButton?.performClick()
        shadowOf(getMainLooper()).idle()

        assertTrue(onConfirmCalled)
        assertFalse(onCancelCalled)
    }

    @Test
    fun `Dialog cancel callback is called correctly`() {
        var onConfirmCalled = false
        var onCancelCalled = false

        val onConfirm = { onConfirmCalled = true }
        val onCancel = { onCancelCalled = true }

        val fragment = spy(SimpleRedirectDialogFragment.newInstance(themeResId = R.style.Theme_AppCompat_Light))
        doNothing().`when`(fragment).dismiss()

        doReturn(testContext).`when`(fragment).requireContext()
        @Suppress("DEPRECATION")
        doReturn(mockFragmentManager()).`when`(fragment).requireFragmentManager()

        fragment.setAppLinkRedirectUrl(webUrl)
        fragment.onConfirmRedirect = onConfirm
        fragment.onCancelRedirect = onCancel

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val confirmButton = dialog.findViewById<Button>(android.R.id.button2)
        confirmButton?.performClick()
        shadowOf(getMainLooper()).idle()

        assertFalse(onConfirmCalled)
        assertTrue(onCancelCalled)
    }

    @Test
    fun `Dialog confirm and cancel is not called when dismissed`() {
        var onConfirmCalled = false
        var onCancelCalled = false

        val onConfirm = { onConfirmCalled = true }
        val onCancel = { onCancelCalled = true }

        val fragment = spy(SimpleRedirectDialogFragment.newInstance(themeResId = R.style.Theme_AppCompat_Light))
        doNothing().`when`(fragment).dismiss()

        doReturn(testContext).`when`(fragment).requireContext()
        @Suppress("DEPRECATION")
        doReturn(mockFragmentManager()).`when`(fragment).requireFragmentManager()

        fragment.setAppLinkRedirectUrl(webUrl)
        fragment.onConfirmRedirect = onConfirm
        fragment.onCancelRedirect = onCancel

        val dialog = fragment.onCreateDialog(null)
        dialog.show()
        dialog.dismiss()

        assertFalse(onConfirmCalled)
        assertFalse(onCancelCalled)
    }

    private fun mockFragmentManager(): FragmentManager {
        val fragmentManager: FragmentManager = mock()
        val transaction: FragmentTransaction = mock()
        doReturn(transaction).`when`(fragmentManager).beginTransaction()
        return fragmentManager
    }
}
