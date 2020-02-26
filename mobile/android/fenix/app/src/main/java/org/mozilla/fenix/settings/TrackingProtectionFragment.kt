/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import androidx.navigation.findNavController
import androidx.preference.CheckBoxPreference
import androidx.preference.DropDownPreference
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar

/**
 * Displays the toggle for tracking protection, options for tracking protection policy and a button
 * to open info about the tracking protection [org.mozilla.fenix.exceptions.ExceptionsFragment].
 */
class TrackingProtectionFragment : PreferenceFragmentCompat() {

    private val exceptionsClickListener = Preference.OnPreferenceClickListener {
        val directions =
            TrackingProtectionFragmentDirections.actionTrackingProtectionFragmentToExceptionsFragment()
        view!!.findNavController().navigate(directions)
        true
    }
    private lateinit var radioStrict: RadioButtonInfoPreference
    private lateinit var radioStandard: RadioButtonInfoPreference
    private lateinit var radioCustom: RadioButtonInfoPreference
    private lateinit var customCookies: CheckBoxPreference
    private lateinit var customCookiesSelect: DropDownPreference
    private lateinit var customTracking: CheckBoxPreference
    private lateinit var customTrackingSelect: DropDownPreference
    private lateinit var customCryptominers: CheckBoxPreference
    private lateinit var customFingerprinters: CheckBoxPreference

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.tracking_protection_preferences, rootKey)
        bindStrict()
        bindStandard()
        bindCustom()
        setupRadioGroups()
        updateCustomOptionsVisibility()
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preference_enhanced_tracking_protection))

        // Tracking Protection Switch
        val trackingProtectionKey = getPreferenceKey(R.string.pref_key_tracking_protection)
        val preferenceTP = findPreference<SwitchPreference>(trackingProtectionKey)

        preferenceTP?.isChecked = requireContext().settings().shouldUseTrackingProtection
        preferenceTP?.setOnPreferenceChangeListener<Boolean> { preference, trackingProtectionOn ->
            preference.context.settings().shouldUseTrackingProtection =
                trackingProtectionOn
            with(preference.context.components) {
                val policy = core.trackingProtectionPolicyFactory
                    .createTrackingProtectionPolicy(trackingProtectionOn)
                useCases.settingsUseCases.updateTrackingProtection(policy)
                useCases.sessionUseCases.reload()
            }
            true
        }

        val trackingProtectionLearnMore =
            context!!.getPreferenceKey(R.string.pref_key_etp_learn_more)
        val learnMorePreference = findPreference<Preference>(trackingProtectionLearnMore)
        learnMorePreference?.setOnPreferenceClickListener {
            (activity as HomeActivity).openToBrowserAndLoad(
                searchTermOrURL = SupportUtils.getGenericSumoURLForTopic
                    (SupportUtils.SumoTopic.TRACKING_PROTECTION),
                newTab = true,
                from = BrowserDirection.FromTrackingProtection
            )
            true
        }
        learnMorePreference?.summary = getString(
            R.string.preference_enhanced_tracking_protection_explanation,
            getString(R.string.app_name)
        )

        val exceptions = getPreferenceKey(R.string.pref_key_tracking_protection_exceptions)
        val preferenceExceptions = findPreference<Preference>(exceptions)
        preferenceExceptions?.onPreferenceClickListener = exceptionsClickListener
    }

    private fun bindStrict() {
        val keyStrict = getString(R.string.pref_key_tracking_protection_strict_default)
        radioStrict = requireNotNull(findPreference(keyStrict))
        radioStrict.contentDescription =
            getString(R.string.preference_enhanced_tracking_protection_strict_info_button)

        radioStrict.onClickListener {
            updateCustomOptionsVisibility()
            updateTrackingProtectionPolicy()
            context?.metrics?.track(
                Event.TrackingProtectionSettingChanged(
                    Event.TrackingProtectionSettingChanged.Setting.STRICT
                )
            )
        }

        radioStrict.onInfoClickListener {
            nav(
                R.id.trackingProtectionFragment,
                TrackingProtectionFragmentDirections
                    .actionTrackingProtectionFragmentToTrackingProtectionBlockingFragment(
                        getString(R.string.preference_enhanced_tracking_protection_strict_default)
                    )
            )
        }
    }

    private fun bindStandard() {
        val keyStandard = getString(R.string.pref_key_tracking_protection_standard_option)
        radioStandard = requireNotNull(findPreference(keyStandard))
        radioStandard.contentDescription =
            getString(R.string.preference_enhanced_tracking_protection_standard_info_button)

        radioStandard.onClickListener {
            updateCustomOptionsVisibility()
            updateTrackingProtectionPolicy()
            context?.metrics?.track(
                Event.TrackingProtectionSettingChanged(
                    Event.TrackingProtectionSettingChanged.Setting.STANDARD
                )
            )
        }

        radioStandard.onInfoClickListener {
            nav(
                R.id.trackingProtectionFragment,
                TrackingProtectionFragmentDirections
                    .actionTrackingProtectionFragmentToTrackingProtectionBlockingFragment(
                        getString(R.string.preference_enhanced_tracking_protection_standard_option)
                    )
            )
        }
    }

    private fun bindCustom() {
        val keyCustom = getString(R.string.pref_key_tracking_protection_custom_option)
        radioCustom = requireNotNull(findPreference(keyCustom))
        radioCustom.contentDescription =
            getString(R.string.preference_enhanced_tracking_protection_custom_info_button)

        radioCustom.onClickListener {
            updateCustomOptionsVisibility()
            updateTrackingProtectionPolicy()
        }
        radioCustom.onInfoClickListener {
            nav(
                R.id.trackingProtectionFragment,
                TrackingProtectionFragmentDirections
                    .actionTrackingProtectionFragmentToTrackingProtectionBlockingFragment(
                        getString(R.string.preference_enhanced_tracking_protection_custom)
                    )
            )
        }

        customCookies = requireNotNull(
            findPreference(
                getString(R.string.pref_key_tracking_protection_custom_cookies)
            )
        )

        customCookiesSelect = requireNotNull(
            findPreference(
                getString(R.string.pref_key_tracking_protection_custom_cookies_select)
            )
        )
        customTracking = requireNotNull(
            findPreference(
                getString(R.string.pref_key_tracking_protection_custom_tracking_content)
            )
        )
        customTrackingSelect = requireNotNull(
            findPreference(
                getString(R.string.pref_key_tracking_protection_custom_tracking_content_select)
            )
        )
        customCryptominers = requireNotNull(
            findPreference(
                getString(R.string.pref_key_tracking_protection_custom_cryptominers)
            )
        )
        customFingerprinters = requireNotNull(
            findPreference(
                getString(R.string.pref_key_tracking_protection_custom_fingerprinters)
            )
        )

        customCookies.onPreferenceChangeListener = object : SharedPreferenceUpdater() {
            override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
                customCookiesSelect.isVisible = !customCookies.isChecked
                return super.onPreferenceChange(preference, newValue).also {
                    updateTrackingProtectionPolicy()
                }
            }
        }

        customTracking.onPreferenceChangeListener = object : SharedPreferenceUpdater() {
            override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
                customTrackingSelect.isVisible = !customTracking.isChecked
                return super.onPreferenceChange(preference, newValue).also {
                    updateTrackingProtectionPolicy()
                }
            }
        }

        customCookiesSelect.onPreferenceChangeListener = object : StringSharedPreferenceUpdater() {
            override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
                return super.onPreferenceChange(preference, newValue).also {
                    updateTrackingProtectionPolicy()
                }
            }
        }

        customTrackingSelect.onPreferenceChangeListener = object : StringSharedPreferenceUpdater() {
            override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {

                return super.onPreferenceChange(preference, newValue).also {
                    updateTrackingProtectionPolicy()
                }
            }
        }

        customCryptominers.onPreferenceChangeListener = object : SharedPreferenceUpdater() {
            override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
                return super.onPreferenceChange(preference, newValue).also {
                    updateTrackingProtectionPolicy()
                }
            }
        }

        customFingerprinters.onPreferenceChangeListener = object : SharedPreferenceUpdater() {
            override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
                return super.onPreferenceChange(preference, newValue).also {
                    updateTrackingProtectionPolicy()
                }
            }
        }

        updateCustomOptionsVisibility()
    }

    private fun updateTrackingProtectionPolicy() {
        context?.components?.let {
            val policy = it.core.trackingProtectionPolicyFactory
                .createTrackingProtectionPolicy()
            it.useCases.settingsUseCases.updateTrackingProtection.invoke(policy)
            it.useCases.sessionUseCases.reload.invoke()
        }
    }

    private fun setupRadioGroups() {
        radioStandard.addToRadioGroup(radioStrict)
        radioStrict.addToRadioGroup(radioStandard)

        radioStandard.addToRadioGroup(radioCustom)
        radioCustom.addToRadioGroup(radioStandard)

        radioStrict.addToRadioGroup(radioCustom)
        radioCustom.addToRadioGroup(radioStrict)
    }

    private fun updateCustomOptionsVisibility() {
        val isCustomSelected = requireContext().settings().useCustomTrackingProtection
        customCookies.isVisible = isCustomSelected
        customCookiesSelect.isVisible = isCustomSelected && customCookies.isChecked
        customTracking.isVisible = isCustomSelected
        customTrackingSelect.isVisible = isCustomSelected && customTracking.isChecked
        customCryptominers.isVisible = isCustomSelected
        customFingerprinters.isVisible = isCustomSelected
    }
}
