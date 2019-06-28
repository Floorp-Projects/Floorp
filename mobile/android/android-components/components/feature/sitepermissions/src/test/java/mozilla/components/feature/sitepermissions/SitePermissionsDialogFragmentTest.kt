/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions

import android.view.Gravity.TOP
import android.view.ViewGroup
import android.widget.Button
import android.widget.CheckBox
import androidx.core.view.isVisible
import androidx.fragment.app.FragmentManager
import androidx.fragment.app.FragmentTransaction
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.feature.sitepermissions.SitePermissionsFeature.PromptsStyling
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class SitePermissionsDialogFragmentTest {

    @Test
    fun `build dialog`() {
        val fragment = spy(
            SitePermissionsDialogFragment.newInstance(
                "sessionId",
                "title",
                R.drawable.notification_icon_background,
                mock(),
                shouldShowDoNotAskAgainCheckBox = true
            )
        )

        doReturn(testContext).`when`(fragment).requireContext()
        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val checkBox = dialog.findViewById<CheckBox>(R.id.do_not_ask_again)

        assertTrue("Checkbox should be displayed", checkBox.isVisible)
        assertFalse("Checkbox shouldn't be checked", checkBox.isChecked)
        assertFalse("User selection property should be false", fragment.userSelectionCheckBox)
    }

    @Test
    fun `display dialog with unselected "don't ask again"`() {
        val fragment = spy(
            SitePermissionsDialogFragment.newInstance(
                "sessionId",
                "title",
                R.drawable.notification_icon_background,
                mock(),
                true,
                shouldSelectDoNotAskAgainCheckBox = false
            )
        )

        doReturn(testContext).`when`(fragment).requireContext()
        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val checkBox = dialog.findViewById<CheckBox>(R.id.do_not_ask_again)

        assertTrue("Checkbox should be displayed", checkBox.isVisible)
        assertFalse("Checkbox shouldn't be checked", checkBox.isChecked)
        assertFalse("User selection property should be false", fragment.userSelectionCheckBox)
    }

    @Test
    fun `display dialog with preselected "don't ask again"`() {
        val fragment = spy(
            SitePermissionsDialogFragment.newInstance(
                "sessionId",
                "title",
                R.drawable.notification_icon_background,
                mock(),
                shouldShowDoNotAskAgainCheckBox = true,
                shouldSelectDoNotAskAgainCheckBox = true
            )
        )

        doReturn(testContext).`when`(fragment).requireContext()
        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val checkBox = dialog.findViewById<CheckBox>(R.id.do_not_ask_again)

        assertTrue("Checkbox should be displayed", checkBox.isVisible)
        assertTrue("Checkbox should be checked", checkBox.isChecked)
        assertTrue("User selection property should be true", fragment.userSelectionCheckBox)
    }

    @Test
    fun `dialog with shouldShowDoNotAskAgainCheckBox equals false should not have a checkbox`() {
        val fragment = spy(
            SitePermissionsDialogFragment.newInstance(
                "sessionId",
                "title",
                R.drawable.notification_icon_background,
                mock(),
                shouldShowDoNotAskAgainCheckBox = false
            )
        )

        doReturn(testContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val checkBox = dialog.findViewById<CheckBox>(R.id.do_not_ask_again)

        assertFalse("Checkbox shouldn't be displayed", checkBox.isVisible)
        assertFalse("User selection property should be false", fragment.userSelectionCheckBox)
    }

    @Test
    fun `clicking on positive button notifies the feature (temporary)`() {
        val mockFeature: SitePermissionsFeature = mock()

        val fragment = spy(
            SitePermissionsDialogFragment.newInstance(
                "sessionId",
                "title",
                R.drawable.notification_icon_background,
                mockFeature,
                shouldShowDoNotAskAgainCheckBox = false,
                shouldSelectDoNotAskAgainCheckBox = false
            )
        )

        fragment.feature = mockFeature

        doReturn(testContext).`when`(fragment).requireContext()
        doReturn(mockFragmentManager()).`when`(fragment).fragmentManager

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val positiveButton = dialog.findViewById<Button>(R.id.allow_button)
        positiveButton.performClick()
        verify(mockFeature).onPositiveButtonPress("sessionId", false)
    }

    @Test
    fun `clicking on negative button notifies the feature (temporary)`() {
        val mockFeature: SitePermissionsFeature = mock()

        val fragment = spy(
            SitePermissionsDialogFragment.newInstance(
                "sessionId",
                "title",
                R.drawable.notification_icon_background,
                mockFeature,
                shouldShowDoNotAskAgainCheckBox = false,
                shouldSelectDoNotAskAgainCheckBox = false
            )
        )

        fragment.feature = mockFeature

        doReturn(testContext).`when`(fragment).requireContext()
        doReturn(mockFragmentManager()).`when`(fragment).fragmentManager

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val positiveButton = dialog.findViewById<Button>(R.id.deny_button)
        positiveButton.performClick()
        verify(mockFeature).onNegativeButtonPress("sessionId", false)
    }

    @Test
    fun `clicking on positive button notifies the feature (permanent)`() {
        val mockFeature: SitePermissionsFeature = mock()

        val fragment = spy(
            SitePermissionsDialogFragment.newInstance(
                "sessionId",
                "title",
                R.drawable.notification_icon_background,
                mockFeature,
                shouldShowDoNotAskAgainCheckBox = false,
                shouldSelectDoNotAskAgainCheckBox = true
            )
        )

        fragment.feature = mockFeature

        doReturn(testContext).`when`(fragment).requireContext()
        doReturn(mockFragmentManager()).`when`(fragment).fragmentManager

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val positiveButton = dialog.findViewById<Button>(R.id.allow_button)
        positiveButton.performClick()
        verify(mockFeature).onPositiveButtonPress("sessionId", true)
    }

    @Test
    fun `clicking on negative button notifies the feature (permanent)`() {
        val mockFeature: SitePermissionsFeature = mock()

        val fragment = spy(
            SitePermissionsDialogFragment.newInstance(
                "sessionId",
                "title",
                R.drawable.notification_icon_background,
                mockFeature,
                shouldShowDoNotAskAgainCheckBox = false,
                shouldSelectDoNotAskAgainCheckBox = true
            )
        )

        fragment.feature = mockFeature

        doReturn(testContext).`when`(fragment).requireContext()
        doReturn(mockFragmentManager()).`when`(fragment).fragmentManager

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val positiveButton = dialog.findViewById<Button>(R.id.deny_button)
        positiveButton.performClick()
        verify(mockFeature).onNegativeButtonPress("sessionId", true)
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
                shouldShowDoNotAskAgainCheckBox = false
            )
        )

        fragment.feature = mockFeature

        doReturn(testContext).`when`(fragment).requireContext()

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
                shouldShowDoNotAskAgainCheckBox = true,
                shouldSelectDoNotAskAgainCheckBox = false,
                isNotificationRequest = true
            )
        )

        doReturn(testContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val checkBox = dialog.findViewById<CheckBox>(R.id.do_not_ask_again)

        assertFalse("Checkbox shouldn't be displayed", checkBox.isVisible)
        assertTrue("User selection property should be true", fragment.userSelectionCheckBox)
    }

    private fun mockFragmentManager(): FragmentManager {
        val fragmentManager: FragmentManager = mock()
        val transaction: FragmentTransaction = mock()
        doReturn(transaction).`when`(fragmentManager).beginTransaction()
        return fragmentManager
    }
}
