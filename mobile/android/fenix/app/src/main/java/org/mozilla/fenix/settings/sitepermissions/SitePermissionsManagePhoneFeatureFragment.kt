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
import androidx.navigation.fragment.navArgs
import kotlinx.android.synthetic.main.fragment_manage_site_permissions_feature_phone.view.*
import mozilla.components.feature.sitepermissions.SitePermissionsRules
import mozilla.components.feature.sitepermissions.SitePermissionsRules.Action.ALLOWED
import mozilla.components.feature.sitepermissions.SitePermissionsRules.Action.ASK_TO_ALLOW
import mozilla.components.feature.sitepermissions.SitePermissionsRules.Action.BLOCKED
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.settings.PhoneFeature.AUTOPLAY_AUDIBLE
import org.mozilla.fenix.settings.PhoneFeature.AUTOPLAY_INAUDIBLE
import org.mozilla.fenix.settings.setStartCheckedIndicator
import org.mozilla.fenix.utils.Settings

const val AUTOPLAY_BLOCK_ALL = 0
const val AUTOPLAY_BLOCK_AUDIBLE = 1
const val AUTOPLAY_ALLOW_ON_WIFI = 2
const val AUTOPLAY_ALLOW_ALL = 3

@SuppressWarnings("TooManyFunctions")
class SitePermissionsManagePhoneFeatureFragment : Fragment() {

    private val args by navArgs<SitePermissionsManagePhoneFeatureFragmentArgs>()
    private val settings by lazy { requireContext().settings() }
    private lateinit var blockedByAndroidView: View

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
        initThirdRadio(rootView)
        initFourthRadio(rootView)
        bindBlockedByAndroidContainer(rootView)

        return rootView
    }

    override fun onResume() {
        super.onResume()
        showToolbar(args.phoneFeature.getLabel(requireContext()))
        initBlockedByAndroidView(args.phoneFeature, blockedByAndroidView)
    }

    private fun initFirstRadio(rootView: View) {
        with(rootView.ask_to_allow_radio) {
            if (args.phoneFeature == AUTOPLAY_AUDIBLE) {
                // Disabled because GV does not allow this setting. TODO Reenable after
                // https://bugzilla.mozilla.org/show_bug.cgi?id=1621825 is fixed
//                text = getString(R.string.preference_option_autoplay_allowed2)
//                setOnClickListener {
//                    saveActionInSettings(it.context, AUTOPLAY_ALLOW_ALL)
//                }
//                restoreState(AUTOPLAY_ALLOW_ALL)
                visibility = View.GONE
            } else {
                text = getCombinedLabel(
                    getString(R.string.preference_option_phone_feature_ask_to_allow),
                    getString(R.string.phone_feature_recommended)
                )
                setOnClickListener {
                    saveActionInSettings(ASK_TO_ALLOW)
                }
                restoreState(ASK_TO_ALLOW)
                visibility = View.VISIBLE
            }
        }
    }

    private fun initSecondRadio(rootView: View) {
        with(rootView.block_radio) {
            if (args.phoneFeature == AUTOPLAY_AUDIBLE) {
                text = getCombinedLabel(
                    getString(R.string.preference_option_autoplay_allowed_wifi_only2),
                    getString(R.string.preference_option_autoplay_allowed_wifi_subtext)
                )
                setOnClickListener {
                    // TODO replace with AUTOPLAY_ALLOW_ON_WIFI when
                    // https://bugzilla.mozilla.org/show_bug.cgi?id=1621825 is fixed. This GV bug
                    // makes ALLOW_ALL behave as ALLOW_ON_WIFI
                    saveActionInSettings(AUTOPLAY_ALLOW_ALL)
                }
                restoreState(AUTOPLAY_ALLOW_ON_WIFI)
            } else {
                text = getString(R.string.preference_option_phone_feature_blocked)
                setOnClickListener {
                    saveActionInSettings(BLOCKED)
                }
                restoreState(BLOCKED)
            }
        }
    }

    private fun initThirdRadio(rootView: View) {
        with(rootView.third_radio) {
            if (args.phoneFeature == AUTOPLAY_AUDIBLE) {
                visibility = View.VISIBLE
                text = getString(R.string.preference_option_autoplay_block_audio2)
                setOnClickListener {
                    saveActionInSettings(AUTOPLAY_BLOCK_AUDIBLE)
                }
                restoreState(AUTOPLAY_BLOCK_AUDIBLE)
            } else {
                visibility = View.GONE
            }
        }
    }

    private fun initFourthRadio(rootView: View) {
        with(rootView.fourth_radio) {
            if (args.phoneFeature == AUTOPLAY_AUDIBLE) {
                visibility = View.VISIBLE
                text = getCombinedLabel(
                    getString(R.string.preference_option_autoplay_blocked3),
                    getString(R.string.phone_feature_recommended)
                )
                setOnClickListener {
                    saveActionInSettings(AUTOPLAY_BLOCK_ALL)
                }
                restoreState(AUTOPLAY_BLOCK_ALL)
            } else {
                visibility = View.GONE
            }
        }
    }

    private fun RadioButton.restoreState(buttonAction: SitePermissionsRules.Action) {
        if (args.phoneFeature.getAction(settings) == buttonAction) {
            this.isChecked = true
            this.setStartCheckedIndicator()
        }
    }

    private fun RadioButton.restoreState(buttonAutoplaySetting: Int) {
        if (settings.getAutoplayUserSetting(AUTOPLAY_BLOCK_ALL) == buttonAutoplaySetting) {
            this.isChecked = true
            this.setStartCheckedIndicator()
        }
    }

    private fun saveActionInSettings(action: SitePermissionsRules.Action) {
        settings.setSitePermissionsPhoneFeatureAction(args.phoneFeature, action)
    }

    /**
     * Saves the user selected autoplay setting.
     *
     * See [Settings.setAutoplayUserSetting] kdoc for an explanation of why this cannot follow the
     * same code path as other permissions.
     */
    private fun saveActionInSettings(autoplaySetting: Int) {
        settings.setAutoplayUserSetting(autoplaySetting)
        val (audible, inaudible) = when (autoplaySetting) {
            AUTOPLAY_ALLOW_ALL,
            AUTOPLAY_ALLOW_ON_WIFI -> {
                settings.setAutoplayUserSetting(AUTOPLAY_ALLOW_ON_WIFI)
                BLOCKED to BLOCKED
            }
            AUTOPLAY_BLOCK_AUDIBLE -> BLOCKED to ALLOWED
            AUTOPLAY_BLOCK_ALL -> BLOCKED to BLOCKED
            else -> return
        }
        settings.setSitePermissionsPhoneFeatureAction(AUTOPLAY_AUDIBLE, audible)
        settings.setSitePermissionsPhoneFeatureAction(AUTOPLAY_INAUDIBLE, inaudible)
    }

    private fun bindBlockedByAndroidContainer(rootView: View) {
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

    /**
     * Returns a [CharSequence] that arranges and styles [mainText], a line break, and then [subText]
     */
    private fun getCombinedLabel(mainText: CharSequence, subText: CharSequence): CharSequence {
        val subTextSize =
            resources.getDimensionPixelSize(R.dimen.phone_feature_label_recommended_text_size)
        val recommendedSpannable = SpannableString(subText)

        recommendedSpannable.setSpan(
            ForegroundColorSpan(Color.GRAY),
            0,
            recommendedSpannable.length,
            SPAN_EXCLUSIVE_INCLUSIVE
        )

        recommendedSpannable.setSpan(
            AbsoluteSizeSpan(subTextSize), 0,
            recommendedSpannable.length,
            SPAN_EXCLUSIVE_INCLUSIVE
        )

        return with(SpannableStringBuilder()) {
            append(mainText)
            append("\n")
            append(recommendedSpannable)
            this
        }
    }
}
