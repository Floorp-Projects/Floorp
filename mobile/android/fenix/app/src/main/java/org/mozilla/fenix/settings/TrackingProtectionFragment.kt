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
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.utils.Settings

/**
 * Displays the toggle for tracking protection and a button to open
 * the tracking protection [org.mozilla.fenix.exceptions.ExceptionsFragment].
 */
class TrackingProtectionFragment : PreferenceFragmentCompat() {

    private val exceptionsClickListener = Preference.OnPreferenceClickListener {
        val directions = TrackingProtectionFragmentDirections.actionTrackingProtectionFragmentToExceptionsFragment()
        view!!.findNavController().navigate(directions)
        true
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.tracking_protection_preferences, rootKey)
    }

    override fun onResume() {
        super.onResume()
        activity?.title = getString(R.string.preferences_tracking_protection)
        (activity as AppCompatActivity).supportActionBar?.show()

        // Tracking Protection Switch
        val trackingProtectionKey = getPreferenceKey(R.string.pref_key_tracking_protection)
        val preferenceTP = findPreference<SwitchPreference>(trackingProtectionKey)

        preferenceTP?.isChecked = Settings.getInstance(context!!).shouldUseTrackingProtection
        preferenceTP?.setOnPreferenceChangeListener<Boolean> { preference, trackingProtectionOn ->
            Settings.getInstance(preference.context).shouldUseTrackingProtection = trackingProtectionOn
            with(preference.context.components) {
                val policy = core.createTrackingProtectionPolicy(trackingProtectionOn)
                useCases.settingsUseCases.updateTrackingProtection(policy)
                useCases.sessionUseCases.reload()
            }
            true
        }

        val exceptions = getPreferenceKey(R.string.pref_key_tracking_protection_exceptions)
        val preferenceExceptions = findPreference<Preference>(exceptions)
        preferenceExceptions?.onPreferenceClickListener = exceptionsClickListener
    }
}
