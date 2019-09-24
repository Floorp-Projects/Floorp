/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.navigation.findNavController
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.settings

/**
 * Displays the toggle for tracking protection and a button to open
 * the tracking protection [org.mozilla.fenix.exceptions.ExceptionsFragment].
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

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.tracking_protection_preferences, rootKey)
    }

    override fun onResume() {
        super.onResume()
        activity?.title = getString(R.string.preference_enhanced_tracking_protection)
        (activity as AppCompatActivity).supportActionBar?.show()

        // Tracking Protection Switch
        val trackingProtectionKey = getPreferenceKey(R.string.pref_key_tracking_protection)
        val preferenceTP = findPreference<SwitchPreference>(trackingProtectionKey)

        preferenceTP?.isChecked = requireContext().settings().shouldUseTrackingProtection
        preferenceTP?.setOnPreferenceChangeListener<Boolean> { preference, trackingProtectionOn ->
            preference.context.settings().shouldUseTrackingProtection =
                trackingProtectionOn
            with(preference.context.components) {
                val policy = core.createTrackingProtectionPolicy(trackingProtectionOn)
                useCases.settingsUseCases.updateTrackingProtection(policy)
                useCases.sessionUseCases.reload()
            }
            true
        }

        bindStrict()
        bindStandard()
        setupRadioGroups()

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
        val keyStrict = getString(R.string.pref_key_tracking_protection_strict)
        radioStrict = requireNotNull(findPreference(keyStrict))
        radioStrict.onPreferenceChangeListener = SharedPreferenceUpdater()
        radioStrict.isVisible = FeatureFlags.etpCategories
        radioStrict.onPreferenceChangeListener = object : SharedPreferenceUpdater() {
            override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
                context?.metrics?.track(
                    Event.TrackingProtectionSettingChanged(
                        Event.TrackingProtectionSettingChanged.Setting.STRICT
                    )
                )
                return super.onPreferenceChange(preference, newValue)
            }
        }
        radioStrict.onInfoClickListener {
            nav(
                R.id.trackingProtectionFragment,
                TrackingProtectionFragmentDirections
                    .actionTrackingProtectionFragmentToTrackingProtectionBlockingFragment(true)
            )
        }
        radioStrict.onClickListener {
            updateTrackingProtectionPolicy()
        }
    }

    private fun bindStandard() {
        val keyStandard = getString(R.string.pref_key_tracking_protection_standard)
        radioStandard = requireNotNull(findPreference(keyStandard))
        radioStandard.isVisible = FeatureFlags.etpCategories
        radioStandard.onPreferenceChangeListener = object : SharedPreferenceUpdater() {
            override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
                context?.metrics?.track(
                    Event.TrackingProtectionSettingChanged(
                        Event.TrackingProtectionSettingChanged.Setting.STANDARD
                    )
                )
                return super.onPreferenceChange(preference, newValue)
            }
        }
        radioStandard.onInfoClickListener {
            nav(
                R.id.trackingProtectionFragment,
                TrackingProtectionFragmentDirections
                    .actionTrackingProtectionFragmentToTrackingProtectionBlockingFragment(false)
            )
        }
        radioStandard.onClickListener {
            updateTrackingProtectionPolicy()
        }
    }

    private fun updateTrackingProtectionPolicy() {
        context?.components?.let {
            val policy = it.core.createTrackingProtectionPolicy()
            it.useCases.settingsUseCases.updateTrackingProtection.invoke(policy)
            it.useCases.sessionUseCases.reload.invoke()
        }
    }

    private fun setupRadioGroups() {
        radioStandard.addToRadioGroup(radioStrict)
        radioStrict.addToRadioGroup(radioStandard)
    }
}
