/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.ui

import android.view.Gravity.TOP
import android.view.ViewGroup
import android.widget.Button
import android.widget.TextView
import androidx.fragment.app.FragmentManager
import androidx.fragment.app.FragmentTransaction
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.R
import mozilla.components.feature.addons.ui.PermissionsDialogFragment.PromptsStyling
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy

@RunWith(AndroidJUnit4::class)
class PermissionsDialogFragmentTest {

    @Test
    fun `build dialog`() {
        val addon = Addon(
            "id",
            translatableName = mapOf(Addon.DEFAULT_LOCALE to "my_addon"),
            permissions = listOf("privacy", "<all_urls>", "tabs"),
        )
        val fragment = createPermissionsDialogFragment(addon)

        doReturn(testContext).`when`(fragment).requireContext()
        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val name = addon.translateName(testContext)
        val titleTextView = dialog.findViewById<TextView>(R.id.title)
        val permissionTextView = dialog.findViewById<TextView>(R.id.permissions)
        val permissionText = fragment.buildPermissionsText()

        assertTrue(titleTextView.text.contains(name))
        assertTrue(permissionText.contains(testContext.getString(R.string.mozac_feature_addons_permissions_dialog_subtitle)))
        assertTrue(permissionText.contains(testContext.getString(R.string.mozac_feature_addons_permissions_privacy_description)))
        assertTrue(permissionText.contains(testContext.getString(R.string.mozac_feature_addons_permissions_all_urls_description)))
        assertTrue(permissionText.contains(testContext.getString(R.string.mozac_feature_addons_permissions_tabs_description)))

        assertTrue(permissionTextView.text.contains(testContext.getString(R.string.mozac_feature_addons_permissions_dialog_subtitle)))
        assertTrue(permissionTextView.text.contains(testContext.getString(R.string.mozac_feature_addons_permissions_privacy_description)))
        assertTrue(permissionTextView.text.contains(testContext.getString(R.string.mozac_feature_addons_permissions_all_urls_description)))
        assertTrue(permissionTextView.text.contains(testContext.getString(R.string.mozac_feature_addons_permissions_tabs_description)))
    }

    @Test
    fun `clicking on dialog buttons notifies lambdas`() {
        val addon = Addon("id", translatableName = mapOf(Addon.DEFAULT_LOCALE to "my_addon"))

        val fragment = createPermissionsDialogFragment(addon)
        var allowedWasExecuted = false
        var denyWasExecuted = false

        fragment.onPositiveButtonClicked = {
            allowedWasExecuted = true
        }

        fragment.onNegativeButtonClicked = {
            denyWasExecuted = true
        }

        doReturn(testContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val positiveButton = dialog.findViewById<Button>(R.id.allow_button)
        val negativeButton = dialog.findViewById<Button>(R.id.deny_button)

        positiveButton.performClick()
        negativeButton.performClick()

        assertTrue(allowedWasExecuted)
        assertTrue(denyWasExecuted)
    }

    @Test
    fun `dismissing the dialog notifies deny lambda`() {
        val addon = Addon("id", translatableName = mapOf(Addon.DEFAULT_LOCALE to "my_addon"))

        val fragment = createPermissionsDialogFragment(addon)
        var denyWasExecuted = false

        fragment.onNegativeButtonClicked = {
            denyWasExecuted = true
        }

        doReturn(testContext).`when`(fragment).requireContext()

        doReturn(mockFragmentManager()).`when`(fragment).parentFragmentManager

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        fragment.onCancel(mock())

        assertTrue(denyWasExecuted)
    }

    @Test
    fun `dialog must have all the styles of the feature promptsStyling object`() {
        val addon = Addon("id", translatableName = mapOf(Addon.DEFAULT_LOCALE to "my_addon"))
        val styling = PromptsStyling(TOP, true)
        val fragment = createPermissionsDialogFragment(addon, styling)

        doReturn(testContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)

        val dialogAttributes = dialog.window!!.attributes

        assertTrue(dialogAttributes.gravity == TOP)
        assertTrue(dialogAttributes.width == ViewGroup.LayoutParams.MATCH_PARENT)
    }

    @Test
    fun `handles add-ons without permissions`() {
        val addon = Addon(
            "id",
            translatableName = mapOf(Addon.DEFAULT_LOCALE to "my_addon"),
            permissions = emptyList(),
        )
        val fragment = createPermissionsDialogFragment(addon)

        doReturn(testContext).`when`(fragment).requireContext()
        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val name = addon.translateName(testContext)
        val titleTextView = dialog.findViewById<TextView>(R.id.title)
        val permissionTextView = dialog.findViewById<TextView>(R.id.permissions)
        val permissionText = fragment.buildPermissionsText()

        assertTrue(titleTextView.text.contains(name))
        assertFalse(permissionText.contains(testContext.getString(R.string.mozac_feature_addons_permissions_dialog_subtitle)))
        assertFalse(permissionTextView.text.contains(testContext.getString(R.string.mozac_feature_addons_permissions_dialog_subtitle)))
    }

    @Test
    fun `build dialog for optional permissions`() {
        val addon = Addon(
            "id",
            translatableName = mapOf(Addon.DEFAULT_LOCALE to "my_addon"),
            permissions = listOf("privacy", "https://example.org/", "tabs"),
        )
        val fragment = createPermissionsDialogFragment(addon, forOptionalPermissions = true, optionalPermissions = addon.permissions)

        doReturn(testContext).`when`(fragment).requireContext()
        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val addonName = addon.translateName(testContext)
        val titleTextView = dialog.findViewById<TextView>(R.id.title)
        val permissionTextView = dialog.findViewById<TextView>(R.id.permissions)
        val allowButton = dialog.findViewById<Button>(R.id.allow_button)
        val denyButton = dialog.findViewById<Button>(R.id.deny_button)
        val permissionText = fragment.buildPermissionsText()

        assertEquals(
            titleTextView.text,
            testContext.getString(R.string.mozac_feature_addons_optional_permissions_dialog_title, addonName),
        )

        assertTrue(permissionText.contains(testContext.getString(R.string.mozac_feature_addons_optional_permissions_dialog_subtitle)))
        assertTrue(permissionText.contains(testContext.getString(R.string.mozac_feature_addons_permissions_privacy_description)))
        assertTrue(
            permissionText.contains(
                testContext.getString(
                    R.string.mozac_feature_addons_permissions_one_site_description,
                    "example.org",
                ),
            ),
        )
        assertTrue(permissionText.contains(testContext.getString(R.string.mozac_feature_addons_permissions_tabs_description)))

        assertTrue(permissionTextView.text.contains(testContext.getString(R.string.mozac_feature_addons_optional_permissions_dialog_subtitle)))
        assertTrue(permissionTextView.text.contains(testContext.getString(R.string.mozac_feature_addons_permissions_privacy_description)))
        assertTrue(
            permissionTextView.text.contains(
                testContext.getString(
                    R.string.mozac_feature_addons_permissions_one_site_description,
                    "example.org",
                ),
            ),
        )
        assertTrue(permissionTextView.text.contains(testContext.getString(R.string.mozac_feature_addons_permissions_tabs_description)))

        assertEquals(allowButton.text, testContext.getString(R.string.mozac_feature_addons_permissions_dialog_allow))
        assertEquals(denyButton.text, testContext.getString(R.string.mozac_feature_addons_permissions_dialog_deny))
    }

    private fun createPermissionsDialogFragment(
        addon: Addon,
        promptsStyling: PromptsStyling? = null,
        forOptionalPermissions: Boolean = false,
        optionalPermissions: List<String> = emptyList(),
    ): PermissionsDialogFragment {
        return spy(
            PermissionsDialogFragment.newInstance(
                addon = addon,
                promptsStyling = promptsStyling,
                forOptionalPermissions = forOptionalPermissions,
                optionalPermissions = optionalPermissions,
            ),
        ).apply {
            doNothing().`when`(this).dismiss()
        }
    }

    private fun mockFragmentManager(): FragmentManager {
        val fragmentManager: FragmentManager = mock()
        val transaction: FragmentTransaction = mock()
        doReturn(transaction).`when`(fragmentManager).beginTransaction()
        return fragmentManager
    }
}
