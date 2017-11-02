/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.os.Bundle;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.preference.PreferenceScreen;
import android.support.annotation.Nullable;
import android.text.TextUtils;

import org.mozilla.focus.R;
import org.mozilla.focus.activity.InfoActivity;
import org.mozilla.focus.activity.SettingsActivity;
import org.mozilla.focus.locale.LocaleManager;
import org.mozilla.focus.locale.Locales;
import org.mozilla.focus.telemetry.TelemetryWrapper;
import org.mozilla.focus.utils.AppConstants;
import org.mozilla.focus.widget.DefaultBrowserPreference;

import java.util.Locale;

public class SettingsFragment extends PreferenceFragment implements SharedPreferences.OnSharedPreferenceChangeListener {
    public static final String FRAGMENT_CLASS_INTENT_EXTRA = "extra_fragment_class";
    public static final String PREFERENCES_RESID_INTENT_EXTRA = "extra_preferences_resid";
    public static final String TITLE_RESID_INTENT_EXTRA = "extra_title_resid";

    public static final int EXTRA_VALUE_NONE = -1;

    private boolean localeUpdated;

    public interface ActionBarUpdater {
        void updateTitle(int titleResId);
        void updateIcon(int iconResId);
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // We've checked that this cast is legal in onAttach.
        final ActionBarUpdater updater = (ActionBarUpdater) getActivity();

        final Bundle args = getArguments();
        int prefResId = R.xml.settings;
        int titleResId = R.string.menu_settings;

        if (args != null) {
            prefResId = args.getInt(PREFERENCES_RESID_INTENT_EXTRA, R.xml.settings);
            titleResId = args.getInt(TITLE_RESID_INTENT_EXTRA, R.string.menu_settings);
        }

        updater.updateTitle(titleResId);
        addPreferencesFromResource(prefResId);
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        if (!(getActivity() instanceof ActionBarUpdater)) {
            throw new IllegalArgumentException("Parent activity must implement ActionBarUpdater");
        }
    }

    @Override
    public boolean onPreferenceTreeClick(PreferenceScreen preferenceScreen, Preference preference) {
        final Resources resources = getResources();

        // AppCompatActivity has a Toolbar that is used as the ActionBar, and it conflicts with the ActionBar
        // used by PreferenceScreen to create the headers (with title, back navigation), so we wrap all these
        // "preference screens" into separate activities.
        if (preference.getKey().equals(resources.getString(R.string.pref_key_about))) {
            final Intent intent = InfoActivity.getAboutIntent(getActivity());
            startActivity(intent);
        } else if (preference.getKey().equals(resources.getString(R.string.pref_key_help))) {
            Intent helpIntent = InfoActivity.getHelpIntent(getActivity());
            startActivity(helpIntent);
        } else if (preference.getKey().equals(resources.getString(R.string.pref_key_rights))) {
            final Intent intent = InfoActivity.getRightsIntent(getActivity());
            startActivity(intent);
        } else if (preference.getKey().equals(resources.getString(R.string.pref_key_privacy_notice))) {
            final Intent intent = InfoActivity.getPrivacyNoticeIntent(getActivity());
            startActivity(intent);
        } else if (preference.getKey().equals(resources.getString(R.string.pref_key_search_engine))) {
            final Intent intent = getSettingsIntent(getActivity(),
                    AppConstants.FLAG_MANUAL_SEARCH_ENGINE ? R.xml.search_engine_settings_featureflag_manual : R.xml.search_engine_settings,
                    R.string.preference_search_installed_search_engines);
            startActivity(intent);
        } else if (preference.getKey().equals(resources.getString(R.string.pref_key_manual_add_search_engine))) {
            final Intent intent = getSettingsIntent(getActivity(), ManualAddSearchEngineSettingsFragment.FRAGMENT_CLASS_TYPE,
                    R.xml.manual_add_search_engine,
                    R.string.tutorial_search_title);
            startActivity(intent);
        }

        return super.onPreferenceTreeClick(preferenceScreen, preference);
    }

    private static Intent getSettingsIntent(Context context, int prefsResId, int titleResId) {
        return getSettingsIntent(context, EXTRA_VALUE_NONE, prefsResId, titleResId);
    }

    private static Intent getSettingsIntent(Context context, int fragmentClassType, int prefsResId, int titleResId) {
        final Intent intent = new Intent(context, SettingsActivity.class);
        intent.putExtra(PREFERENCES_RESID_INTENT_EXTRA, prefsResId);
        intent.putExtra(TITLE_RESID_INTENT_EXTRA, titleResId);

        if (fragmentClassType != EXTRA_VALUE_NONE) {
            intent.putExtra(FRAGMENT_CLASS_INTENT_EXTRA, fragmentClassType);
        }
        return intent;
    }

    @Override
    public void onResume() {
        super.onResume();

        getPreferenceManager().getSharedPreferences().registerOnSharedPreferenceChangeListener(this);

        final DefaultBrowserPreference preference = (DefaultBrowserPreference) findPreference(getString(R.string.pref_key_default_browser));
        if (preference != null) {
            preference.update();
        }
    }

    @Override
    public void onPause() {
        super.onPause();

        getPreferenceManager().getSharedPreferences().unregisterOnSharedPreferenceChangeListener(this);
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
                    .replace(R.id.container, new SettingsFragment())
                    .commit();
        }
    }
}
