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
import androidx.annotation.IdRes
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AlertDialog
import androidx.core.view.isVisible
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.navArgs
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.launch
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.concept.engine.permission.SitePermissions.Status.ALLOWED
import mozilla.components.concept.engine.permission.SitePermissions.Status.BLOCKED
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.settings.quicksettings.AutoplayValue
import org.mozilla.fenix.settings.setStartCheckedIndicator
import org.mozilla.fenix.settings.update
import org.mozilla.fenix.utils.Settings

@SuppressWarnings("TooManyFunctions")
class SitePermissionsManageExceptionsPhoneFeatureFragment : Fragment() {

    private lateinit var radioAllow: RadioButton
    private lateinit var radioBlock: RadioButton
    private lateinit var blockedByAndroidView: View

    @VisibleForTesting
    internal lateinit var rootView: View
    private val args by navArgs<SitePermissionsManageExceptionsPhoneFeatureFragmentArgs>()

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View? {
        rootView =
            inflater.inflate(R.layout.fragment_manage_site_permissions_exceptions_feature_phone, container, false)

        if (getFeature() == PhoneFeature.AUTOPLAY) {
            initAutoplay(getSitePermission())
        } else {
            initNormalFeature()
        }

        bindBlockedByAndroidContainer()
        initClearPermissionsButton()
        return rootView
    }

    @VisibleForTesting
    internal fun getFeature(): PhoneFeature = args.phoneFeature

    @VisibleForTesting
    internal fun getSitePermission(): SitePermissions = args.sitePermissions

    @VisibleForTesting
    internal fun getSettings(): Settings = requireContext().settings()

    fun initAutoplay(sitePermissions: SitePermissions? = null) {
        val context = requireContext()
        val autoplayValues = AutoplayValue.values(context, getSettings(), sitePermissions)
        val allowAudioAndVideo =
            requireNotNull(autoplayValues.find { it is AutoplayValue.AllowAll })
        val blockAll = requireNotNull(autoplayValues.find { it is AutoplayValue.BlockAll })
        val blockAudible = requireNotNull(autoplayValues.find { it is AutoplayValue.BlockAudible })

        initAutoplayOption(R.id.ask_to_allow_radio, allowAudioAndVideo)
        initAutoplayOption(R.id.block_radio, blockAll)
        initAutoplayOption(R.id.optional_radio, blockAudible)
    }

    fun initNormalFeature() {
        initAskToAllowRadio(rootView)
        initBlockRadio()
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getFeature().getLabel(requireContext()))
        initBlockedByAndroidView(getFeature(), blockedByAndroidView)
    }

    @VisibleForTesting
    internal fun initAutoplayOption(@IdRes viewId: Int, value: AutoplayValue) {
        val radio = rootView.findViewById<RadioButton>(viewId)
        radio.isVisible = true
        radio.text = value.label

        radio.setOnClickListener {
            updatedSitePermissions(value)
        }
        radio.restoreState(value)
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
        val permissionsStatus = getFeature().getStatus(getSitePermission())
        if (permissionsStatus != SitePermissions.Status.NO_DECISION && permissionsStatus == status) {
            this.isChecked = true
            this.setStartCheckedIndicator()
        }
    }

    @VisibleForTesting
    internal fun RadioButton.restoreState(autoplayValue: AutoplayValue) {
        if (autoplayValue.isSelected()) {
            this.isChecked = true
            this.setStartCheckedIndicator()
        }
    }

    private fun initBlockRadio() {
        radioBlock = rootView.findViewById(R.id.block_radio)
        radioBlock.setOnClickListener {
            updatedSitePermissions(BLOCKED)
        }
        radioBlock.restoreState(BLOCKED)
    }

    @VisibleForTesting
    internal fun initClearPermissionsButton() {
        val button = rootView.findViewById<Button>(R.id.reset_permission)
        button.setText(R.string.clear_permission)
        button.setOnClickListener {
            AlertDialog.Builder(requireContext()).apply {
                setMessage(R.string.confirm_clear_permission_site)
                setTitle(R.string.clear_permission)
                setPositiveButton(R.string.clear_permission_positive) { dialog: DialogInterface, _ ->
                    clearPermissions()
                    dialog.dismiss()
                }
                setNegativeButton(R.string.clear_permission_negative) { dialog: DialogInterface, _ ->
                    dialog.cancel()
                }
            }.show()
        }
    }

    private fun clearPermissions() {
        if (getFeature() == PhoneFeature.AUTOPLAY) {
            clearAutoplayPermissions()
        } else {
            clearNormalPermissions()
        }
    }

    private fun clearAutoplayPermissions() {
        val context = requireContext()
        val settings = context.settings()
        val defaultValue =
            AutoplayValue.values(context, settings, null).firstOrNull { it.isSelected() }
                ?: AutoplayValue.getFallbackValue(
                    context,
                    settings,
                    null,
                )
        updatedSitePermissions(defaultValue)
        initAutoplay()
    }

    private fun clearNormalPermissions() {
        val defaultStatus = getFeature().getStatus(settings = getSettings())
        updatedSitePermissions(defaultStatus)
        resetRadioButtonsStatus(defaultStatus)
    }

    private fun resetRadioButtonsStatus(defaultStatus: SitePermissions.Status) {
        radioAllow.isChecked = false
        radioBlock.isChecked = false
        radioAllow.restoreState(defaultStatus)
        radioBlock.restoreState(defaultStatus)
    }

    @VisibleForTesting
    internal fun bindBlockedByAndroidContainer() {
        blockedByAndroidView = rootView.findViewById(R.id.permissions_blocked_container)
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

    private fun updatedSitePermissions(status: SitePermissions.Status) {
        val updatedSitePermissions = getSitePermission().update(getFeature(), status)
        viewLifecycleOwner.lifecycleScope.launch(Main) {
            requireComponents.core.permissionStorage.updateSitePermissions(
                sitePermissions = updatedSitePermissions,
                private = false,
            )
            requireComponents.tryReloadTabBy(updatedSitePermissions.origin)
        }
    }

    @VisibleForTesting
    internal fun updatedSitePermissions(autoplayValue: AutoplayValue) {
        val updatedSitePermissions = autoplayValue.updateSitePermissions(getSitePermission())
        viewLifecycleOwner.lifecycleScope.launch(Main) {
            requireComponents.core.permissionStorage.updateSitePermissions(
                sitePermissions = updatedSitePermissions,
                private = false,
            )
            requireComponents.tryReloadTabBy(updatedSitePermissions.origin)
        }
    }
}
