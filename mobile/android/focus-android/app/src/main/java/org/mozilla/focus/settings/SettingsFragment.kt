/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import android.content.SharedPreferences
import android.os.Bundle
import android.support.v7.preference.ListPreference
import android.text.TextUtils
import org.mozilla.focus.R
import org.mozilla.focus.activity.SettingsActivity
import org.mozilla.focus.locale.LocaleManager
import org.mozilla.focus.locale.Locales
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.widget.DefaultBrowserPreference
import java.util.*

class SettingsFragment : BaseSettingsFragment(), SharedPreferences.OnSharedPreferenceChangeListener {

    private var localeUpdated: Boolean = false

    @Suppress("ForbiddenComment")
    override fun onCreatePreferences(bundle: Bundle?, s: String?) {
        addPreferencesFromResource(R.xml.settings)
        // TODO: #2869 Enable Debugging and Expose Advanced Setting
        preferenceScreen.removePreference(findPreference(getString(R.string.pref_key_advanced_screen)))
    }

    override fun onResume() {
        super.onResume()

        preferenceManager.sharedPreferences.registerOnSharedPreferenceChangeListener(this)

        val preference = findPreference(getString(R.string.pref_key_default_browser)) as DefaultBrowserPreference
        preference.update()

        // Update title and icons when returning to fragments.
        val updater = activity as BaseSettingsFragment.ActionBarUpdater?
        updater!!.updateTitle(R.string.menu_settings)
        updater.updateIcon(R.drawable.ic_back)
    }

    override fun onPause() {
        preferenceManager.sharedPreferences.unregisterOnSharedPreferenceChangeListener(this)
        super.onPause()
    }

    override fun onPreferenceTreeClick(preference: android.support.v7.preference.Preference): Boolean {
        val resources = resources

        when {
            preference.key == resources.getString(R.string
                    .pref_key_privacy_security_screen) -> navigateToFragment(PrivacySecuritySettingsFragment())
            preference.key == resources.getString(R.string
                    .pref_key_search_screen) -> navigateToFragment(SearchSettingsFragment())
            preference.key == resources.getString(R.string
                    .pref_key_advanced_screen) -> navigateToFragment(AdvancedSettingsFragment())
            preference.key == resources.getString(R.string
                    .pref_key_mozilla_screen) -> navigateToFragment(MozillaSettingsFragment())
        }

        return super.onPreferenceTreeClick(preference)
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences, key: String) {
        TelemetryWrapper.settingsEvent(key, sharedPreferences.all[key].toString())

        if (!localeUpdated && key == getString(R.string.pref_key_locale)) {
            // Updating the locale leads to onSharedPreferenceChanged being triggered again in some
            // cases. To avoid an infinite loop we won't update the preference a second time. This
            // fragment gets replaced at the end of this method anyways.
            localeUpdated = true

            //Set langChanged from InstalledSearchEngines to true
            InstalledSearchEnginesSettingsFragment.languageChanged = true

            val languagePreference = findPreference(getString(R.string.pref_key_locale)) as ListPreference
            val value = languagePreference.value

            val localeManager = LocaleManager.getInstance()

            val locale: Locale?
            if (TextUtils.isEmpty(value)) {
                localeManager.resetToSystemLocale(activity)
                locale = localeManager.getCurrentLocale(activity)
            } else {
                locale = Locales.parseLocaleCode(value)
                localeManager.setSelectedLocale(activity, value)
            }
            localeManager.updateConfiguration(activity, locale)

            // Manually notify SettingsActivity of locale changes (in most other cases activities
            // will detect changes in onActivityResult(), but that doesn't apply to SettingsActivity).
            activity!!.onConfigurationChanged(activity!!.resources.configuration)

            // And ensure that the calling LocaleAware*Activity knows that the locale changed:
            activity!!.setResult(SettingsActivity.ACTIVITY_RESULT_LOCALE_CHANGED)

            // The easiest way to ensure we update the language is by replacing the entire fragment:
            fragmentManager!!.beginTransaction()
                    .replace(R.id.container, SettingsFragment.newInstance())
                    .commit()
        }
    }

    companion object {

        fun newInstance(): SettingsFragment {
            return SettingsFragment()
        }
    }
}
