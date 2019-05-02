/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions

import android.content.Context
import android.view.ContextThemeWrapper
import android.view.Gravity.TOP
import android.view.ViewGroup
import android.widget.Button
import android.widget.CheckBox
import androidx.fragment.app.FragmentManager
import androidx.fragment.app.FragmentTransaction
import androidx.test.core.app.ApplicationProvider
import mozilla.components.feature.sitepermissions.SitePermissionsFeature.PromptsStyling
import mozilla.components.support.ktx.android.view.isVisible
import mozilla.components.support.test.mock
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class SitePermissionsDialogFragmentTest {

    private val context: Context
        get() = ContextThemeWrapper(
            ApplicationProvider.getApplicationContext(),
            R.style.Theme_AppCompat
        )

    @Test
    fun `build dialog`() {
        val fragment = spy(
            SitePermissionsDialogFragment.newInstance(
                "sessionId",
                "title",
                R.drawable.notification_icon_background,
                mock(),
                true
            )
        )

        doReturn(context).`when`(fragment).requireContext()
        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val checkBox = dialog.findViewById<CheckBox>(R.id.do_not_ask_again)

        assertTrue(checkBox.isVisible())
    }

    @Test
    fun `dialog with shouldIncludeCheckBox equals false should not have a checkbox`() {
        val fragment = spy(
            SitePermissionsDialogFragment.newInstance(
                "sessionId",
                "title",
                R.drawable.notification_icon_background,
                mock(),
                false
            )
        )

        doReturn(context).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val checkBox = dialog.findViewById<CheckBox>(R.id.do_not_ask_again)

        assertFalse(checkBox.isVisible())
    }

    @Test
    fun `clicking on positive button notifies the feature`() {
        val mockFeature: SitePermissionsFeature = mock()

        val fragment = spy(
            SitePermissionsDialogFragment.newInstance(
                "sessionId",
                "title",
                R.drawable.notification_icon_background,
                mockFeature,
                false
            )
        )

        fragment.feature = mockFeature

        doReturn(context).`when`(fragment).requireContext()
        doReturn(mockFragmentManager()).`when`(fragment).fragmentManager

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val positiveButton = dialog.findViewById<Button>(R.id.allow_button)
        positiveButton.performClick()
        verify(mockFeature).onPositiveButtonPress("sessionId", false)
    }

    @Test
    fun `clicking on negative button notifies the feature`() {
        val mockFeature: SitePermissionsFeature = mock()

        val fragment = spy(
            SitePermissionsDialogFragment.newInstance(
                "sessionId",
                "title",
                R.drawable.notification_icon_background,
                mockFeature,
                false
            )
        )

        fragment.feature = mockFeature

        doReturn(context).`when`(fragment).requireContext()
        doReturn(mockFragmentManager()).`when`(fragment).fragmentManager

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val positiveButton = dialog.findViewById<Button>(R.id.deny_button)
        positiveButton.performClick()
        verify(mockFeature).onNegativeButtonPress("sessionId", false)
    }

    @Test
    fun `dialog must have all the styles of the feature promptsStyling object`() {
        val mockFeature: SitePermissionsFeature = mock()

        doReturn(PromptsStyling(TOP, true)).`when`(mockFeature).promptsStyling

        val fragment = spy(
            SitePermissionsDialogFragment.newInstance(
                "sessionId",
                "title",
                R.drawable.notification_icon_background,
                mockFeature,
                false
            )
        )

        fragment.feature = mockFeature

        doReturn(context).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)

        val dialogAttributes = dialog.window!!.attributes

        assertTrue(dialogAttributes.gravity == TOP)
        assertTrue(dialogAttributes.width == ViewGroup.LayoutParams.MATCH_PARENT)
    }

    @Test
    fun `dialog with isNotificationRequest equals true should not have a checkbox`() {
        val fragment = spy(
            SitePermissionsDialogFragment.newInstance(
                "sessionId",
                "title",
                R.drawable.notification_icon_background,
                mock(),
                shouldIncludeCheckBox = true,
                isNotificationRequest = true
            )
        )

        doReturn(context).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val checkBox = dialog.findViewById<CheckBox>(R.id.do_not_ask_again)

        assertFalse(checkBox.isVisible())
    }

    private fun mockFragmentManager(): FragmentManager {
        val fragmentManager: FragmentManager = mock()
        val transaction: FragmentTransaction = mock()
        doReturn(transaction).`when`(fragmentManager).beginTransaction()
        return fragmentManager
    }
}