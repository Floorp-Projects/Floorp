/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Intent
import android.graphics.Color
import android.net.Uri
import android.os.Bundle
import android.provider.Settings.ACTION_APPLICATION_DETAILS_SETTINGS
import android.text.SpannableString
import android.text.SpannableStringBuilder
import android.text.Spanned.SPAN_EXCLUSIVE_INCLUSIVE
import android.text.style.AbsoluteSizeSpan
import android.text.style.ForegroundColorSpan
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.RadioButton
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import mozilla.components.feature.sitepermissions.SitePermissionsRules
import mozilla.components.feature.sitepermissions.SitePermissionsRules.Action.ASK_TO_ALLOW
import mozilla.components.feature.sitepermissions.SitePermissionsRules.Action.BLOCKED
import org.mozilla.fenix.R
import org.mozilla.fenix.utils.Settings

@SuppressWarnings("TooManyFunctions")
class SitePermissionsManagePhoneFeatureFragment : Fragment() {
    private lateinit var phoneFeature: PhoneFeature
    private lateinit var settings: Settings
    private lateinit var blockedByAndroidView: View

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        phoneFeature = SitePermissionsManagePhoneFeatureFragmentArgs
            .fromBundle(requireArguments())
            .permission.toPhoneFeature()

        (activity as AppCompatActivity).title = phoneFeature.getLabel(requireContext())
        (activity as AppCompatActivity).supportActionBar?.show()
        settings = Settings.getInstance(requireContext())
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        val rootView = inflater.inflate(R.layout.fragment_manage_site_permissions_feature_phone, container, false)

        initAskToAllowRadio(rootView)
        initBlockRadio(rootView)
        bindBlockedByAndroidContainer(rootView)

        return rootView
    }
    override fun onResume() {
        super.onResume()
        initBlockedByAndroidView(phoneFeature, blockedByAndroidView)
    }

    private fun initAskToAllowRadio(rootView: View) {
        val radio = rootView.findViewById<RadioButton>(R.id.ask_to_allow_radio)
        val askToAllowText = getString(R.string.preference_option_phone_feature_ask_to_allow)
        val recommendedText = getString(R.string.phone_feature_recommended)
        val recommendedTextSize = resources.getDimensionPixelSize(R.dimen.phone_feature_label_recommended_text_size)
        val recommendedSpannable = SpannableString(recommendedText)

        recommendedSpannable.setSpan(
            ForegroundColorSpan(Color.GRAY),
            0,
            recommendedSpannable.length,
            SPAN_EXCLUSIVE_INCLUSIVE
        )

        recommendedSpannable.setSpan(
            AbsoluteSizeSpan(recommendedTextSize), 0,
            recommendedSpannable.length,
            SPAN_EXCLUSIVE_INCLUSIVE
        )

        radio.text = with(SpannableStringBuilder()) {
            append(askToAllowText)
            append("\n")
            append(recommendedSpannable)
            this
        }
        radio.setOnClickListener {
            saveActionInSettings(ASK_TO_ALLOW)
        }
        radio.restoreState(ASK_TO_ALLOW)
    }

    private fun RadioButton.restoreState(action: SitePermissionsRules.Action) {
        if (phoneFeature.action == action) {
            this.isChecked = true
            this.setStartCheckedIndicator()
        }
    }

    private fun initBlockRadio(rootView: View) {
        val radio = rootView.findViewById<RadioButton>(R.id.block_radio)
        radio.setOnClickListener {
            saveActionInSettings(BLOCKED)
        }
        radio.restoreState(BLOCKED)
    }

    private fun bindBlockedByAndroidContainer(rootView: View) {
        blockedByAndroidView = rootView.findViewById<View>(R.id.permissions_blocked_container)
        initSettingsButton(blockedByAndroidView)
    }

    private fun Int.toPhoneFeature(): PhoneFeature {
        return requireNotNull(PhoneFeature.values().find { feature ->
            this == feature.id
        }) {
            "$this is a invalid PhoneFeature"
        }
    }

    private val PhoneFeature.action: SitePermissionsRules.Action
        get() {
            return when (phoneFeature) {
                PhoneFeature.CAMERA -> settings.sitePermissionsPhoneFeatureCameraAction
                PhoneFeature.LOCATION -> settings.sitePermissionsPhoneFeatureLocation
                PhoneFeature.MICROPHONE -> settings.sitePermissionsPhoneFeatureMicrophoneAction
                PhoneFeature.NOTIFICATION -> settings.sitePermissionsPhoneFeatureNotificationAction
            }
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

    private fun saveActionInSettings(action: SitePermissionsRules.Action) {
        when (phoneFeature) {
            PhoneFeature.CAMERA -> settings.sitePermissionsPhoneFeatureCameraAction = action
            PhoneFeature.LOCATION -> settings.sitePermissionsPhoneFeatureLocation = action
            PhoneFeature.MICROPHONE -> settings.sitePermissionsPhoneFeatureMicrophoneAction = action
            PhoneFeature.NOTIFICATION -> settings.sitePermissionsPhoneFeatureNotificationAction = action
        }
    }
}
