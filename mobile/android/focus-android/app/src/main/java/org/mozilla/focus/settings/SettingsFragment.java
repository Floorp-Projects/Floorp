/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings;

import android.app.Fragment;
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
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import org.mozilla.focus.R;
import org.mozilla.focus.activity.InfoActivity;
import org.mozilla.focus.activity.SettingsActivity;
import org.mozilla.focus.autocomplete.AutocompleteSettingsFragment;
import org.mozilla.focus.locale.LocaleManager;
import org.mozilla.focus.locale.Locales;
import org.mozilla.focus.search.MultiselectSearchEngineListPreference;
import org.mozilla.focus.search.RadioSearchEngineListPreference;
import org.mozilla.focus.search.SearchEngineManager;
import org.mozilla.focus.telemetry.TelemetryWrapper;
import org.mozilla.focus.widget.DefaultBrowserPreference;

import java.util.Locale;
import java.util.Set;

public class SettingsFragment extends PreferenceFragment implements SharedPreferences.OnSharedPreferenceChangeListener {
    public static final String SETTINGS_SCREEN_NAME = "settingsScreenName";

    private boolean localeUpdated;
    private SettingsScreen settingsScreen;

    public interface ActionBarUpdater {
        void updateTitle(int titleResId);
        void updateIcon(int iconResId);
    }

    public enum SettingsScreen {
        MAIN(R.xml.settings, R.string.menu_settings),
        SEARCH_ENGINES(R.xml.search_engine_settings,
                R.string.preference_search_installed_search_engines),
        ADD_SEARCH(R.xml.manual_add_search_engine, R.string.tutorial_search_title),
        REMOVE_ENGINES(R.xml.remove_search_engines, R.string.preference_search_remove_title);

        public final int prefsResId;
        public final int titleResId;

        SettingsScreen(int prefsResId, int titleResId) {
            this.prefsResId = prefsResId;
            this.titleResId =  titleResId;
        }
    }

    public static SettingsFragment newInstance(Bundle intentArgs, SettingsScreen settingsType) {
        final SettingsFragment f;
        switch (settingsType) {
            case MAIN:
            case SEARCH_ENGINES:
            case REMOVE_ENGINES:
                f = new SettingsFragment();
                break;
            case ADD_SEARCH:
                f = new ManualAddSearchEngineSettingsFragment();
                break;
            default:
                throw new IllegalArgumentException("Unknown SettingsScreen type " + settingsType.name());
        }
        if (intentArgs == null) {
            intentArgs = new Bundle();
        }
        intentArgs.putString(SETTINGS_SCREEN_NAME, settingsType.name());
        f.setArguments(intentArgs);
        return f;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        settingsScreen = SettingsScreen.valueOf(getArguments().getString(SETTINGS_SCREEN_NAME, SettingsScreen.MAIN.name()));
        addPreferencesFromResource(settingsScreen.prefsResId);

        setHasOptionsMenu(settingsScreen == SettingsScreen.SEARCH_ENGINES
                || settingsScreen == SettingsScreen.REMOVE_ENGINES);
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        if (!(getActivity() instanceof ActionBarUpdater)) {
            throw new IllegalArgumentException("Parent activity must implement ActionBarUpdater");
        }
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        switch (settingsScreen) {
            case SEARCH_ENGINES:
                inflater.inflate(R.menu.menu_search_engines, menu);
                break;
            case REMOVE_ENGINES:
                inflater.inflate(R.menu.menu_remove_search_engines, menu);
                break;
            default:
                return;
        }
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.menu_remove_search_engines:
                showSettingsFragment(SettingsScreen.REMOVE_ENGINES);
                return true;
            case R.id.menu_delete_items:
                final Preference pref = getPreferenceScreen()
                        .findPreference(getResources().getString(
                                R.string.pref_key_multiselect_search_engine_list));
                final Set<String> enginesToRemove = ((MultiselectSearchEngineListPreference) pref).getCheckedEngineIds();
                SearchEngineManager.removeSearchEngines(enginesToRemove, getSearchEngineSharedPreferences());
                getFragmentManager().popBackStack();
                return true;
            case R.id.menu_restore_default_engines:
                SearchEngineManager.restoreDefaultSearchEngines(getSearchEngineSharedPreferences());
                refetchSearchEngines();
                return true;
            default:
                return super.onOptionsItemSelected(item);
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
            showSettingsFragment(SettingsScreen.SEARCH_ENGINES);
        } else if (preference.getKey().equals(resources.getString(R.string.pref_key_manual_add_search_engine))) {
            showSettingsFragment(SettingsScreen.ADD_SEARCH);
        } else if (preference.getKey().equals(resources.getString(R.string.pref_key_screen_autocomplete))) {
            getFragmentManager().beginTransaction()
                    .replace(R.id.container, new AutocompleteSettingsFragment())
                    .addToBackStack(null)
                    .commit();
        }

        return super.onPreferenceTreeClick(preferenceScreen, preference);
    }

    private void showSettingsFragment(SettingsScreen screenType) {
        final Fragment fragment = SettingsFragment.newInstance(null, screenType);
        getFragmentManager().beginTransaction()
                .replace(R.id.container, fragment)
                .addToBackStack(null)
                .commit();
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
        updater.updateTitle(settingsScreen.titleResId);
        if (settingsScreen == SettingsScreen.REMOVE_ENGINES) {
            updater.updateIcon(R.drawable.ic_close);
        } else {
            updater.updateIcon(R.drawable.ic_back);
        }
        if (settingsScreen == SettingsScreen.SEARCH_ENGINES || settingsScreen == SettingsScreen.REMOVE_ENGINES) {
            refetchSearchEngines();
        }
    }

    /**
     * Refresh search engines list. Only runs if showing the "Installed search engines" screen.
     */
    private void refetchSearchEngines() {
        if (settingsScreen == SettingsScreen.SEARCH_ENGINES) {
            final Preference pref = getPreferenceScreen()
                    .findPreference(getResources().getString(
                            R.string.pref_key_radio_search_engine_list));
            ((RadioSearchEngineListPreference) pref).refetchSearchEngines();

            // Refresh this preference screen to display changes.
            getPreferenceScreen().removeAll();
            addPreferencesFromResource(settingsScreen.prefsResId);
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
                    .replace(R.id.container, SettingsFragment.newInstance(null, SettingsScreen.MAIN))
                    .commit();
        }
    }

    protected SharedPreferences getSearchEngineSharedPreferences() {
        return getActivity().getSharedPreferences(SearchEngineManager.PREF_FILE_SEARCH_ENGINES, Context.MODE_PRIVATE);
    }
}
