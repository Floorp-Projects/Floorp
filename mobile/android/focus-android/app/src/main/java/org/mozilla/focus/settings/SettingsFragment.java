/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings;

import android.content.SharedPreferences;
import android.content.res.Resources;
import android.os.Bundle;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceScreen;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import org.mozilla.focus.R;
import org.mozilla.focus.activity.SettingsActivity;
import org.mozilla.focus.autocomplete.AutocompleteSettingsFragment;
import org.mozilla.focus.browser.LocalizedContent;
import org.mozilla.focus.locale.LocaleManager;
import org.mozilla.focus.locale.Locales;
import org.mozilla.focus.session.SessionManager;
import org.mozilla.focus.session.Source;
import org.mozilla.focus.telemetry.TelemetryWrapper;
import org.mozilla.focus.utils.AppConstants;
import org.mozilla.focus.utils.SupportUtils;
import org.mozilla.focus.widget.DefaultBrowserPreference;

import java.util.Locale;

public class SettingsFragment extends BaseSettingsFragment implements SharedPreferences.OnSharedPreferenceChangeListener {

    private boolean localeUpdated;

    public static SettingsFragment newInstance() {
        return new SettingsFragment();
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        addPreferencesFromResource(R.xml.settings);
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
    public boolean onPreferenceTreeClick(PreferenceScreen preferenceScreen, Preference preference) {
        final Resources resources = getResources();

        // AppCompatActivity has a Toolbar that is used as the ActionBar, and it conflicts with the ActionBar
        // used by PreferenceScreen to create the headers (with title, back navigation), so we wrap all these
        // "preference screens" into separate activities.
        if (preference.getKey().equals(resources.getString(R.string.pref_key_about))) {
            SessionManager.getInstance().createSession(Source.MENU, LocalizedContent.URL_ABOUT);
            getActivity().onBackPressed();
        } else if (preference.getKey().equals(resources.getString(R.string.pref_key_help))) {
            SessionManager.getInstance().createSession(Source.MENU, SupportUtils.HELP_URL);
            getActivity().onBackPressed();
        } else if (preference.getKey().equals(resources.getString(R.string.pref_key_rights))) {
            SessionManager.getInstance().createSession(Source.MENU, LocalizedContent.URL_RIGHTS);
            getActivity().onBackPressed();
        } else if (preference.getKey().equals(resources.getString(R.string.pref_key_privacy_notice))) {
            SessionManager.getInstance().createSession(Source.MENU, AppConstants.isKlarBuild() ?
                    SupportUtils.PRIVACY_NOTICE_KLAR_URL : SupportUtils.PRIVACY_NOTICE_URL);
            getActivity().onBackPressed();
        } else if (preference.getKey().equals(resources.getString(R.string.pref_key_search_engine))) {
            navigateToFragment(new InstalledSearchEnginesSettingsFragment());
            TelemetryWrapper.openSearchSettingsEvent();
        } else if (preference.getKey().equals(resources.getString(R.string.pref_key_screen_autocomplete))) {
            navigateToFragment(new AutocompleteSettingsFragment());
        }

        return super.onPreferenceTreeClick(preferenceScreen, preference);
    }

    @Override
    public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
        TelemetryWrapper.settingsEvent(key, String.valueOf(sharedPreferences.getAll().get(key)));

        if (!localeUpdated && key.equals(getString(R.string.pref_key_locale))) {
            // Updating the locale leads to onSharedPreferenceChanged being triggered again in some
            // cases. To avoid an infinite loop we won't update the preference a second time. This
            // fragment gets replaced at the end of this method anyways.
            localeUpdated = true;

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
