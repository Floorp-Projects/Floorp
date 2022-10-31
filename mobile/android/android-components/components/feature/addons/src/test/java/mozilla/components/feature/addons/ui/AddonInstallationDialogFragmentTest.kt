/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.ui

import android.graphics.Bitmap
import android.view.Gravity
import android.view.ViewGroup
import android.widget.Button
import android.widget.ImageView
import android.widget.TextView
import androidx.appcompat.widget.AppCompatCheckBox
import androidx.fragment.app.FragmentManager
import androidx.fragment.app.FragmentTransaction
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.Job
import kotlinx.coroutines.test.runTest
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.R
import mozilla.components.feature.addons.amo.AddonCollectionProvider
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.whenever
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import java.io.IOException

@RunWith(AndroidJUnit4::class)
class AddonInstallationDialogFragmentTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    @Test
    fun `build dialog`() {
        val addon = Addon(
            "id",
            translatableName = mapOf(Addon.DEFAULT_LOCALE to "my_addon"),
            permissions = listOf("privacy", "<all_urls>", "tabs"),
        )
        val mockedCollectionProvider = mock<AddonCollectionProvider>()
        val fragment = createAddonInstallationDialogFragment(addon, mockedCollectionProvider)
        assertSame(mockedCollectionProvider, fragment.addonCollectionProvider)
        assertSame(addon, fragment.arguments?.getParcelable(KEY_INSTALLED_ADDON))

        doReturn(testContext).`when`(fragment).requireContext()
        val dialog = fragment.onCreateDialog(null)
        dialog.show()
        val name = addon.translateName(testContext)
        val titleTextView = dialog.findViewById<TextView>(R.id.title)
        val description = dialog.findViewById<TextView>(R.id.description)
        val allowedInPrivateBrowsing = dialog.findViewById<AppCompatCheckBox>(R.id.allow_in_private_browsing)

        assertTrue(titleTextView.text.contains(name))
        assertTrue(description.text.contains(testContext.getString(R.string.mozac_feature_addons_installed_dialog_description)))
        assertTrue(allowedInPrivateBrowsing.text.contains(testContext.getString(R.string.mozac_feature_addons_settings_allow_in_private_browsing)))
    }

    @Test
    fun `clicking on confirm dialog buttons notifies lambda with private browsing boolean`() {
        val addon = Addon("id", translatableName = mapOf(Addon.DEFAULT_LOCALE to "my_addon"))

        val fragment = createAddonInstallationDialogFragment(addon, mock())
        var confirmationWasExecuted = false
        var allowInPrivateBrowsing = false

        fragment.onConfirmButtonClicked = { _, allow ->
            confirmationWasExecuted = true
            allowInPrivateBrowsing = allow
        }

        doReturn(testContext).`when`(fragment).requireContext()

        @Suppress("DEPRECATION")
        doReturn(mockFragmentManager()).`when`(fragment).requireFragmentManager()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()
        val confirmButton = dialog.findViewById<Button>(R.id.confirm_button)
        val allowedInPrivateBrowsing = dialog.findViewById<AppCompatCheckBox>(R.id.allow_in_private_browsing)
        confirmButton.performClick()
        assertTrue(confirmationWasExecuted)
        assertFalse(allowInPrivateBrowsing)

        dialog.show()
        allowedInPrivateBrowsing.performClick()
        confirmButton.performClick()
        assertTrue(confirmationWasExecuted)
        assertTrue(allowInPrivateBrowsing)
    }

    @Test
    fun `dismissing the dialog notifies nothing`() {
        val addon = Addon("id", translatableName = mapOf(Addon.DEFAULT_LOCALE to "my_addon"))
        val fragment = createAddonInstallationDialogFragment(addon, mock())
        var confirmationWasExecuted = false

        fragment.onConfirmButtonClicked = { _, _ ->
            confirmationWasExecuted = true
        }

        doReturn(testContext).`when`(fragment).requireContext()

        @Suppress("DEPRECATION")
        doReturn(mockFragmentManager()).`when`(fragment).requireFragmentManager()
        doReturn(mockFragmentManager()).`when`(fragment).getParentFragmentManager()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()
        fragment.onDismiss(mock())
        assertFalse(confirmationWasExecuted)
    }

    @Test
    fun `dialog must have all the styles of the feature promptsStyling object`() {
        val addon = Addon("id", translatableName = mapOf(Addon.DEFAULT_LOCALE to "my_addon"))
        val styling = AddonInstallationDialogFragment.PromptsStyling(Gravity.TOP, true)
        val fragment = createAddonInstallationDialogFragment(addon, mock(), styling)

        doReturn(testContext).`when`(fragment).requireContext()
        val dialog = fragment.onCreateDialog(null)
        val dialogAttributes = dialog.window!!.attributes

        assertTrue(dialogAttributes.gravity == Gravity.TOP)
        assertTrue(dialogAttributes.width == ViewGroup.LayoutParams.MATCH_PARENT)
    }

    @Test
    fun `fetching the add-on icon successfully`() = runTest {
        val addon = mock<Addon>()
        val bitmap = mock<Bitmap>()
        val mockedImageView = spy(ImageView(testContext))
        val mockedCollectionProvider = mock<AddonCollectionProvider>()
        val fragment = createAddonInstallationDialogFragment(addon, mockedCollectionProvider)

        whenever(mockedCollectionProvider.getAddonIconBitmap(addon)).thenReturn(bitmap)
        assertNull(fragment.arguments?.getParcelable<Bitmap>(KEY_ICON))
        fragment.fetchIcon(addon, mockedImageView, scope).join()
        assertNotNull(fragment.arguments?.getParcelable<Bitmap>(KEY_ICON))
        verify(mockedImageView).setImageDrawable(Mockito.any())
    }

    @Test
    fun `handle errors while fetching the add-on icon`() = runTest {
        val addon = mock<Addon>()
        val mockedImageView = spy(ImageView(testContext))
        val mockedCollectionProvider = mock<AddonCollectionProvider>()
        val fragment = createAddonInstallationDialogFragment(addon, mockedCollectionProvider)

        whenever(mockedCollectionProvider.getAddonIconBitmap(addon)).then {
            throw IOException("Request failed")
        }
        try {
            fragment.fetchIcon(addon, mockedImageView, scope).join()
            verify(mockedImageView).setColorFilter(Mockito.anyInt())
        } catch (e: IOException) {
            fail("The exception must be handle in the adapter")
        }
    }

    @Test
    fun `allows state loss when committing`() {
        val addon = mock<Addon>()
        val mockedCollectionProvider = mock<AddonCollectionProvider>()
        val fragment = createAddonInstallationDialogFragment(addon, mockedCollectionProvider)

        val fragmentManager = mock<FragmentManager>()
        val fragmentTransaction = mock<FragmentTransaction>()
        `when`(fragmentManager.beginTransaction()).thenReturn(fragmentTransaction)

        fragment.show(fragmentManager, "test")
        verify(fragmentTransaction).commitAllowingStateLoss()
    }

    @Test
    fun `cancels icon job on stop`() {
        val addon = mock<Addon>()
        val mockedCollectionProvider = mock<AddonCollectionProvider>()
        val fragment = createAddonInstallationDialogFragment(addon, mockedCollectionProvider)

        val job = mock<Job>()
        fragment.iconJob = job
        fragment.onStop()
        verify(job).cancel()
    }

    private fun createAddonInstallationDialogFragment(
        addon: Addon,
        addonCollectionProvider: AddonCollectionProvider,
        promptsStyling: AddonInstallationDialogFragment.PromptsStyling? = null,
    ): AddonInstallationDialogFragment {
        return spy(AddonInstallationDialogFragment.newInstance(addon, addonCollectionProvider, promptsStyling = promptsStyling)).apply {
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
