/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.navigation.Navigation
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.utils.Settings

class TrackingProtectionFragment : PreferenceFragmentCompat() {
    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.tracking_protection_preferences, rootKey)
    }

    override fun onResume() {
        super.onResume()
        activity?.title = getString(R.string.preferences_tracking_protection)
        (activity as AppCompatActivity).supportActionBar?.show()

        // Tracking Protection Switch
        val trackingProtectionKey =
            context!!.getPreferenceKey(R.string.pref_key_tracking_protection)
        val preferenceTP = findPreference<SwitchPreference>(trackingProtectionKey)

        preferenceTP?.isChecked = Settings.getInstance(context!!).shouldUseTrackingProtection
        preferenceTP?.onPreferenceChangeListener =
            Preference.OnPreferenceChangeListener { _, newValue ->
                Settings.getInstance(requireContext()).shouldUseTrackingProtection = newValue as Boolean
                with(requireComponents) {
                    val policy = core.createTrackingProtectionPolicy(newValue)
                    useCases.settingsUseCases.updateTrackingProtection.invoke(policy)
                    useCases.sessionUseCases.reload.invoke()
                }
                true
            }

        val exceptions =
            context!!.getPreferenceKey(R.string.pref_key_tracking_protection_exceptions)
        val preferenceExceptions = findPreference<Preference>(exceptions)
        preferenceExceptions?.onPreferenceClickListener = getClickListenerForExceptions()
    }

    private fun getClickListenerForExceptions(): Preference.OnPreferenceClickListener {
        return Preference.OnPreferenceClickListener {
            val directions =
                TrackingProtectionFragmentDirections.actionTrackingProtectionFragmentToExceptionsFragment()
            Navigation.findNavController(view!!).navigate(directions)
            true
        }
    }
}
