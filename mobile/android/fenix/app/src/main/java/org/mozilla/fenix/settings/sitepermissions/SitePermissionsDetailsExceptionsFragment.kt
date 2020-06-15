/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.sitepermissions

import android.content.DialogInterface
import android.os.Bundle
import androidx.appcompat.app.AlertDialog
import androidx.lifecycle.lifecycleScope
import androidx.navigation.findNavController
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.feature.sitepermissions.SitePermissions
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.settings.PhoneFeature.CAMERA
import org.mozilla.fenix.settings.PhoneFeature.LOCATION
import org.mozilla.fenix.settings.PhoneFeature.MICROPHONE
import org.mozilla.fenix.settings.PhoneFeature.NOTIFICATION
import org.mozilla.fenix.settings.requirePreference

class SitePermissionsDetailsExceptionsFragment : PreferenceFragmentCompat() {
    private lateinit var sitePermissions: SitePermissions

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        sitePermissions = SitePermissionsDetailsExceptionsFragmentArgs
            .fromBundle(requireArguments())
            .sitePermissions
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.site_permissions_details_exceptions_preferences, rootKey)
    }

    override fun onResume() {
        super.onResume()
        showToolbar(sitePermissions.origin)
        viewLifecycleOwner.lifecycleScope.launch(IO) {
            val context = requireContext()
            sitePermissions =
                requireNotNull(context.components.core.permissionStorage.findSitePermissionsBy(sitePermissions.origin))
            withContext(Main) {
                bindCategoryPhoneFeatures()
            }
        }
    }

    private fun bindCategoryPhoneFeatures() {
        initPhoneFeature(CAMERA)
        initPhoneFeature(LOCATION)
        initPhoneFeature(MICROPHONE)
        initPhoneFeature(NOTIFICATION)
        bindClearPermissionsButton()
    }

    private fun initPhoneFeature(phoneFeature: PhoneFeature) {
        val summary = phoneFeature.getActionLabel(requireContext(), sitePermissions)
        val keyPreference = phoneFeature.getPreferenceKey(requireContext())
        val cameraPhoneFeatures: Preference = requireNotNull(findPreference(keyPreference))
        cameraPhoneFeatures.summary = summary

        cameraPhoneFeatures.onPreferenceClickListener = Preference.OnPreferenceClickListener {
            navigateToPhoneFeature(phoneFeature)
            true
        }
    }

    private fun bindClearPermissionsButton() {
        val button: Preference = requirePreference(R.string.pref_key_exceptions_clear_site_permissions)

        button.onPreferenceClickListener = Preference.OnPreferenceClickListener {
            AlertDialog.Builder(requireContext()).apply {
                setMessage(R.string.confirm_clear_permissions_site)
                setTitle(R.string.clear_permissions)
                setPositiveButton(android.R.string.yes) { dialog: DialogInterface, _ ->
                    clearSitePermissions()
                    dialog.dismiss()
                }
                setNegativeButton(android.R.string.no) { dialog: DialogInterface, _ ->
                    dialog.cancel()
                }
            }.show()

            true
        }
    }

    private fun clearSitePermissions() {
        viewLifecycleOwner.lifecycleScope.launch(IO) {
            requireContext().components.core.permissionStorage.deleteSitePermissions(sitePermissions)
            withContext(Main) {
                requireView().findNavController().popBackStack()
                requireContext().components.tryReloadTabBy(sitePermissions.origin)
            }
        }
    }

    private fun navigateToPhoneFeature(phoneFeature: PhoneFeature) {
        val directions =
            SitePermissionsDetailsExceptionsFragmentDirections.actionSitePermissionsToExceptionsToManagePhoneFeature(
                phoneFeature = phoneFeature,
                sitePermissions = sitePermissions
            )
        requireView().findNavController().navigate(directions)
    }
}
