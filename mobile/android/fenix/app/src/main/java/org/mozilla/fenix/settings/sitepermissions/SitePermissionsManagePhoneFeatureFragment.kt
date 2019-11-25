/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.sitepermissions

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
import androidx.fragment.app.Fragment
import mozilla.components.feature.sitepermissions.SitePermissionsRules
import mozilla.components.feature.sitepermissions.SitePermissionsRules.Action.ASK_TO_ALLOW
import mozilla.components.feature.sitepermissions.SitePermissionsRules.Action.BLOCKED
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.settings.initBlockedByAndroidView
import org.mozilla.fenix.settings.setStartCheckedIndicator
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

        showToolbar(phoneFeature.getLabel(requireContext()))
        settings = requireContext().settings()
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val rootView = inflater.inflate(
            R.layout.fragment_manage_site_permissions_feature_phone,
            container,
            false
        )

        initFirstRadio(rootView)
        initSecondRadio(rootView)
        bindBlockedByAndroidContainer(rootView)

        return rootView
    }

    override fun onResume() {
        super.onResume()
        initBlockedByAndroidView(phoneFeature, blockedByAndroidView)
    }

    private fun initFirstRadio(rootView: View) {
        val radio = rootView.findViewById<RadioButton>(R.id.ask_to_allow_radio)
        val askToAllowText = when (phoneFeature) {
            PhoneFeature.AUTOPLAY -> getString(R.string.preference_option_autoplay_blocked)
            else -> getString(R.string.preference_option_phone_feature_ask_to_allow)
        }
        val recommendedText = getString(R.string.phone_feature_recommended)
        val recommendedTextSize =
            resources.getDimensionPixelSize(R.dimen.phone_feature_label_recommended_text_size)
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
        val expectedAction = if (phoneFeature == PhoneFeature.AUTOPLAY) BLOCKED else ASK_TO_ALLOW
        radio.setOnClickListener {
            if (phoneFeature == PhoneFeature.AUTOPLAY) {
                settings.setSitePermissionsPhoneFeatureAction(PhoneFeature.AUTOPLAY, expectedAction)
                requireComponents.core.engine.settings.allowAutoplayMedia = false
            } else {
                saveActionInSettings(expectedAction)
            }
        }
        radio.restoreState(expectedAction)
    }

    private fun RadioButton.restoreState(action: SitePermissionsRules.Action) {
        if (phoneFeature.getAction(settings) == action) {
            this.isChecked = true
            this.setStartCheckedIndicator()
        }
    }

    private fun initSecondRadio(rootView: View) {
        val radio = rootView.findViewById<RadioButton>(R.id.block_radio)
        radio.text = when (phoneFeature) {
            PhoneFeature.AUTOPLAY -> getString(R.string.preference_option_autoplay_allowed)
            else -> getString(R.string.preference_option_phone_feature_blocked)
        }
        val expectedAction = if (phoneFeature == PhoneFeature.AUTOPLAY) ASK_TO_ALLOW else BLOCKED
        radio.setOnClickListener {
            if (phoneFeature == PhoneFeature.AUTOPLAY) {
                settings.setSitePermissionsPhoneFeatureAction(PhoneFeature.AUTOPLAY, expectedAction)
                requireComponents.core.engine.settings.allowAutoplayMedia = true
            } else {
                saveActionInSettings(expectedAction)
            }
        }
        radio.restoreState(expectedAction)
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
        settings.setSitePermissionsPhoneFeatureAction(phoneFeature, action)
    }
}
