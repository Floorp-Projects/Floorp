/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.sitepermissions

import android.content.DialogInterface
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.provider.Settings.ACTION_APPLICATION_DETAILS_SETTINGS
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.RadioButton
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.launch
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.feature.sitepermissions.SitePermissions.Status.ALLOWED
import mozilla.components.feature.sitepermissions.SitePermissions.Status.BLOCKED
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.settings.initBlockedByAndroidView
import org.mozilla.fenix.settings.setStartCheckedIndicator

@SuppressWarnings("TooManyFunctions")
class SitePermissionsManageExceptionsPhoneFeatureFragment : Fragment() {
    private lateinit var phoneFeature: PhoneFeature
    private lateinit var sitePermissions: SitePermissions
    private lateinit var radioAllow: RadioButton
    private lateinit var radioBlock: RadioButton
    private lateinit var blockedByAndroidView: View
    val settings by lazy { requireContext().settings() }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        phoneFeature = SitePermissionsManageExceptionsPhoneFeatureFragmentArgs
            .fromBundle(requireArguments())
            .phoneFeatureId.toPhoneFeature()

        sitePermissions = SitePermissionsManageExceptionsPhoneFeatureFragmentArgs
            .fromBundle(requireArguments())
            .sitePermissions

        (activity as AppCompatActivity).title = phoneFeature.getLabel(requireContext())
        (activity as AppCompatActivity).supportActionBar?.show()
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val rootView =
            inflater.inflate(R.layout.fragment_manage_site_permissions_exceptions_feature_phone, container, false)

        initAskToAllowRadio(rootView)
        initBlockRadio(rootView)
        bindBlockedByAndroidContainer(rootView)
        initClearPermissionsButton(rootView)

        return rootView
    }

    override fun onResume() {
        super.onResume()
        initBlockedByAndroidView(phoneFeature, blockedByAndroidView)
    }

    private fun initAskToAllowRadio(rootView: View) {
        radioAllow = rootView.findViewById(R.id.ask_to_allow_radio)
        val askToAllowText = getString(R.string.preference_option_phone_feature_allowed)

        radioAllow.text = askToAllowText

        radioAllow.setOnClickListener {
            updatedSitePermissions(ALLOWED)
        }
        radioAllow.restoreState(ALLOWED)
    }

    private fun RadioButton.restoreState(status: SitePermissions.Status) {
        if (phoneFeature.getStatus(sitePermissions) == status) {
            this.isChecked = true
            this.setStartCheckedIndicator()
        }
    }

    private fun initBlockRadio(rootView: View) {
        radioBlock = rootView.findViewById(R.id.block_radio)
        radioBlock.setOnClickListener {
            updatedSitePermissions(BLOCKED)
        }
        radioBlock.restoreState(BLOCKED)
    }

    private fun initClearPermissionsButton(rootView: View) {
        val button = rootView.findViewById<Button>(R.id.reset_permission)
        button.setText(R.string.clear_permission)
        button.setOnClickListener {
            AlertDialog.Builder(requireContext()).apply {
                setMessage(R.string.confirm_clear_permission_site)
                setTitle(R.string.clear_permission)
                setPositiveButton(android.R.string.yes) { dialog: DialogInterface, _ ->
                    val defaultStatus = phoneFeature.getStatus(settings = settings)
                    updatedSitePermissions(defaultStatus)
                    resetRadioButtonsStatus(defaultStatus)
                    dialog.dismiss()
                }
                setNegativeButton(android.R.string.no) { dialog: DialogInterface, _ ->
                    dialog.cancel()
                }
            }.show()
        }
    }

    private fun resetRadioButtonsStatus(defaultStatus: SitePermissions.Status) {
        radioAllow.isChecked = false
        radioBlock.isChecked = false
        radioAllow.restoreState(defaultStatus)
        radioBlock.restoreState(defaultStatus)
    }

    private fun bindBlockedByAndroidContainer(rootView: View) {
        blockedByAndroidView = rootView.findViewById<View>(R.id.permissions_blocked_container)
        initSettingsButton(blockedByAndroidView)
    }

    private fun initSettingsButton(rootView: View) {
        val button = rootView.findViewById<Button>(R.id.settings_button)
        button.setOnClickListener {
            openSettings()
        }
    }

    private fun openSettings() {
        val intent = Intent(ACTION_APPLICATION_DETAILS_SETTINGS)
        val uri = Uri.fromParts("package", requireContext().packageName, null)
        intent.data = uri
        startActivity(intent)
    }

    private fun Int.toPhoneFeature(): PhoneFeature {
        return requireNotNull(PhoneFeature.values().find { feature ->
            this == feature.id
        }) {
            "$this is a invalid PhoneFeature"
        }
    }

    private fun updatedSitePermissions(status: SitePermissions.Status) {
        val updatedSitePermissions = when (phoneFeature) {
            PhoneFeature.CAMERA -> sitePermissions.copy(camera = status)
            PhoneFeature.LOCATION -> sitePermissions.copy(location = status)
            PhoneFeature.MICROPHONE -> sitePermissions.copy(microphone = status)
            PhoneFeature.NOTIFICATION -> sitePermissions.copy(notification = status)
            PhoneFeature.AUTOPLAY -> sitePermissions.copy() // not supported by GV or A-C yet
        }
        lifecycleScope.launch(IO) {
            requireComponents.core.permissionStorage.updateSitePermissions(updatedSitePermissions)
        }
    }
}
