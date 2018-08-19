/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings;

import android.content.SharedPreferences;
import android.content.res.Resources;
import android.os.Bundle;
import android.support.v7.preference.ListPreference;
import android.text.TextUtils;
import org.mozilla.focus.R;
import org.mozilla.focus.activity.SettingsActivity;
import org.mozilla.focus.locale.LocaleManager;
import org.mozilla.focus.locale.Locales;
import org.mozilla.focus.telemetry.TelemetryWrapper;
import org.mozilla.focus.widget.DefaultBrowserPreference;

import java.util.Locale;

public class SettingsFragment extends BaseSettingsFragment implements SharedPreferences.OnSharedPreferenceChangeListener {

    private boolean localeUpdated;

    public static SettingsFragment newInstance() {
        return new SettingsFragment();
    }

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        addPreferencesFromResource(R.xml.settings);
        // TODO: #2869 Enable Debugging and Expose Advanced Setting
        getPreferenceScreen().removePreference(findPreference(getString(R.string.pref_key_advanced_screen)));
    }

    @Override
    public void onResume() {
        super.onResume();

        getPreferenceManager().getSharedPreferences().registerOnSharedPreferenceChangeListener(this);

        final DefaultBrowserPreference preference = (DefaultBrowserPreference) findPreference(getString(R.string.pref_key_default_browser));
        if (preference != null) {
            preference.update();
        }

        // Update title and icons when returning to fragments.
        final ActionBarUpdater updater = (ActionBarUpdater) getActivity();
        updater.updateTitle(R.string.menu_settings);
        updater.updateIcon(R.drawable.ic_back);
    }

    @Override
    public void onPause() {
        getPreferenceManager().getSharedPreferences().unregisterOnSharedPreferenceChangeListener(this);
        super.onPause();
    }

    @Override
    public boolean onPreferenceTreeClick(android.support.v7.preference.Preference preference) {
        final Resources resources = getResources();

        if (preference.getKey().equals(resources.getString(R.string
                .pref_key_privacy_security_screen))) {
            navigateToFragment(new PrivacySecuritySettingsFragment());
        } else if (preference.getKey().equals(resources.getString(R.string
                .pref_key_search_screen))) {
            navigateToFragment(new SearchSettingsFragment());
        } else if (preference.getKey().equals(resources.getString(R.string
                .pref_key_advanced_screen))) {
            navigateToFragment(new AdvancedSettingsFragment());
        } else if (preference.getKey().equals(resources.getString(R.string
                .pref_key_mozilla_screen))) {
            navigateToFragment(new MozillaSettingsFragment());
        }

        return super.onPreferenceTreeClick(preference);
    }

    @Override
    public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
        TelemetryWrapper.settingsEvent(key, String.valueOf(sharedPreferences.getAll().get(key)));

        if (!localeUpdated && key.equals(getString(R.string.pref_key_locale))) {
            // Updating the locale leads to onSharedPreferenceChanged being triggered again in some
            // cases. To avoid an infinite loop we won't update the preference a second time. This
            // fragment gets replaced at the end of this method anyways.
            localeUpdated = true;

            //Set langChanged from InstalledSearchEngines to true
            InstalledSearchEnginesSettingsFragment.Companion.setLanguageChanged(true);

            final ListPreference languagePreference = (ListPreference) findPreference(getString(R.string.pref_key_locale));
            final String value = languagePreference.getValue();

            final LocaleManager localeManager = LocaleManager.getInstance();

            final Locale locale;
            if (TextUtils.isEmpty(value)) {
                localeManager.resetToSystemLocale(getActivity());
                locale = localeManager.getCurrentLocale(getActivity());
            } else {
                locale = Locales.parseLocaleCode(value);
                localeManager.setSelectedLocale(getActivity(), value);
            }
            localeManager.updateConfiguration(getActivity(), locale);

            // Manually notify SettingsActivity of locale changes (in most other cases activities
            // will detect changes in onActivityResult(), but that doesn't apply to SettingsActivity).
            getActivity().onConfigurationChanged(getActivity().getResources().getConfiguration());

            // And ensure that the calling LocaleAware*Activity knows that the locale changed:
            getActivity().setResult(SettingsActivity.ACTIVITY_RESULT_LOCALE_CHANGED);

            // The easiest way to ensure we update the language is by replacing the entire fragment:
            getFragmentManager().beginTransaction()
                    .replace(R.id.container, SettingsFragment.newInstance())
                    .commit();
        }
    }
}
