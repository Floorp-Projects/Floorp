/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.Manifest.permission.ACCESS_COARSE_LOCATION
import android.Manifest.permission.CAMERA
import android.Manifest.permission.RECORD_AUDIO
import android.Manifest.permission.ACCESS_FINE_LOCATION
import android.content.Intent
import android.graphics.Color
import android.net.Uri
import android.os.Bundle
import android.provider.Settings.ACTION_APPLICATION_DETAILS_SETTINGS
import org.mozilla.fenix.utils.Settings
import android.text.SpannableString
import android.text.SpannableStringBuilder
import android.text.Spanned.SPAN_EXCLUSIVE_INCLUSIVE
import android.text.style.AbsoluteSizeSpan
import android.text.style.ForegroundColorSpan
import android.view.LayoutInflater
import android.view.View
import android.view.View.VISIBLE
import android.view.ViewGroup
import android.widget.Button
import android.widget.RadioButton
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.text.HtmlCompat
import androidx.core.text.HtmlCompat.FROM_HTML_MODE_COMPACT
import androidx.fragment.app.Fragment
import mozilla.components.feature.sitepermissions.SitePermissionsRules
import mozilla.components.feature.sitepermissions.SitePermissionsRules.Action.BLOCKED
import mozilla.components.feature.sitepermissions.SitePermissionsRules.Action.ASK_TO_ALLOW
import mozilla.components.support.ktx.android.content.isPermissionGranted
import org.mozilla.fenix.R

class SitePermissionsManagePhoneFeature : Fragment() {

    private lateinit var phoneFeature: PhoneFeature
    private lateinit var settings: Settings

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        phoneFeature = SitePermissionsManagePhoneFeatureArgs
            .fromBundle(requireArguments())
            .permission.toPhoneFeature()

        (activity as AppCompatActivity).title = phoneFeature.label
        (activity as AppCompatActivity).supportActionBar?.show()
        settings = Settings.getInstance(requireContext())
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val rootView = inflater.inflate(R.layout.fragment_manage_site_permissions_feature_phone, container, false)

        initAskToAllowRadio(rootView)
        initBlockRadio(rootView)
        initBockedByAndroidContainer(rootView)

        return rootView
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
        }
    }

    private fun initBlockRadio(rootView: View) {
        val radio = rootView.findViewById<RadioButton>(R.id.block_radio)
        radio.setOnClickListener {
            saveActionInSettings(BLOCKED)
        }
        radio.restoreState(BLOCKED)
    }

    private fun initBockedByAndroidContainer(rootView: View) {
        if (!phoneFeature.isAndroidPermissionGranted) {
            val containerView = rootView.findViewById<View>(R.id.permissions_blocked_container)
            containerView.visibility = VISIBLE

            val descriptionLabel = rootView.findViewById<TextView>(R.id.blocked_by_android_explanation_label)
            val text = getString(R.string.phone_feature_blocked_by_android_explanation, phoneFeature.label)
            descriptionLabel.text = HtmlCompat.fromHtml(text, FROM_HTML_MODE_COMPACT)

            initSettingsButton(rootView)
        }
    }

    enum class PhoneFeature(val id: Int) {
        CAMERA(CAMERA_PERMISSION),
        LOCATION(LOCATION_PERMISSION),
        MICROPHONE(MICROPHONE_PERMISSION),
        NOTIFICATION(NOTIFICATION_PERMISSION)
    }

    private val PhoneFeature.label: String
        get() {
            return when (this) {
                PhoneFeature.CAMERA -> getString(R.string.preference_phone_feature_camera)
                PhoneFeature.LOCATION -> getString(R.string.preference_phone_feature_location)
                PhoneFeature.MICROPHONE -> getString(R.string.preference_phone_feature_microphone)
                PhoneFeature.NOTIFICATION -> getString(R.string.preference_phone_feature_notification)
            }
        }

    @Suppress("SpreadOperator")
    private val PhoneFeature.isAndroidPermissionGranted: Boolean
        get() {
            val permissions = when (this) {
                PhoneFeature.CAMERA -> arrayOf(CAMERA)
                PhoneFeature.LOCATION -> arrayOf(ACCESS_COARSE_LOCATION, ACCESS_FINE_LOCATION)
                PhoneFeature.MICROPHONE -> arrayOf(RECORD_AUDIO)
                PhoneFeature.NOTIFICATION -> {
                    return true
                }
            }
            return requireContext().isPermissionGranted(*permissions)
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
                PhoneFeature.CAMERA -> settings.getSitePermissionsPhoneFeatureCameraAction()
                PhoneFeature.LOCATION -> settings.getSitePermissionsPhoneFeatureLocation()
                PhoneFeature.MICROPHONE -> settings.getSitePermissionsPhoneFeatureMicrophoneAction()
                PhoneFeature.NOTIFICATION -> settings.getSitePermissionsPhoneFeatureNotificationAction()
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
            PhoneFeature.CAMERA -> settings.setSitePermissionsPhoneFeatureCameraAction(action)
            PhoneFeature.LOCATION -> settings.setSitePermissionsPhoneFeatureLocation(action)
            PhoneFeature.MICROPHONE -> settings.setSitePermissionsPhoneFeatureMicrophoneAction(action)
            PhoneFeature.NOTIFICATION -> settings.setSitePermissionsPhoneFeatureNotificationAction(action)
        }
    }

    companion object {
        const val CAMERA_PERMISSION = 0
        const val LOCATION_PERMISSION = 1
        const val MICROPHONE_PERMISSION = 2
        const val NOTIFICATION_PERMISSION = 3
    }
}
