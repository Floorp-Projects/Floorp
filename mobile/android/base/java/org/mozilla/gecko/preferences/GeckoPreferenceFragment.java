/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import java.util.Locale;

import org.mozilla.gecko.BrowserLocaleManager;
import org.mozilla.gecko.GeckoApplication;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.LocaleManager;
import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.TelemetryContract.Method;
import org.mozilla.gecko.fxa.AccountLoader;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;

import android.accounts.Account;
import android.app.Activity;
import android.app.LoaderManager;
import android.content.Context;
import android.content.Loader;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.os.Bundle;
import android.preference.PreferenceFragment;
import android.preference.PreferenceScreen;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;

/* A simple implementation of PreferenceFragment for large screen devices
 * This will strip category headers (so that they aren't shown to the user twice)
 * as well as initializing Gecko prefs when a fragment is shown.
*/
public class GeckoPreferenceFragment extends PreferenceFragment {

    public static final int ACCOUNT_LOADER_ID = 1;
    public static final int HEADER_ID_UNDEFINED = -1;
    private AccountLoaderCallbacks accountLoaderCallbacks;
    private SyncPreference syncPreference;

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        Log.d(LOGTAG, "onConfigurationChanged: " + newConfig.locale);

        final Activity context = getActivity();

        final LocaleManager localeManager = BrowserLocaleManager.getInstance();
        final Locale changed = localeManager.onSystemConfigurationChanged(context, getResources(), newConfig, lastLocale);
        if (changed != null) {
            applyLocale(changed);
        }
    }

    private static final String LOGTAG = "GeckoPreferenceFragment";
    private PrefsHelper.PrefHandler mPrefsRequest;
    private Locale lastLocale = Locale.getDefault();

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Write prefs to our custom GeckoSharedPrefs file.
        getPreferenceManager().setSharedPreferencesName(GeckoSharedPrefs.APP_PREFS_NAME);

        int res = getResource();
        if (res == R.xml.preferences) {
            Telemetry.startUISession(TelemetryContract.Session.SETTINGS);
        } else {
            final String resourceName = getArguments().getString("resource");
            Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, Method.SETTINGS, resourceName);
        }

        // Display a menu for Search preferences.
        if (res == R.xml.preferences_search) {
            setHasOptionsMenu(true);
        }

        addPreferencesFromResource(res);

        PreferenceScreen screen = getPreferenceScreen();
        setPreferenceScreen(screen);
        mPrefsRequest = ((GeckoPreferences)getActivity()).setupPreferences(screen);
        syncPreference = (SyncPreference) findPreference(GeckoPreferences.PREFS_SYNC);
    }

    /**
     * Return the title to use for this preference fragment.
     *
     * The result will be used to set the title that you see on non-multi-pane devices.
     */
    private String getTitle() {
        final int res = getResource();
        if (res == R.xml.preferences) {
            return getString(R.string.settings_title);
        } else if (res == R.xml.preferences_general || res == R.xml.preferences_general_tablet) {
            return getString(R.string.pref_category_general);
        } else if (res == R.xml.preferences_home) {
            return getString(R.string.pref_category_home);
        } else if (res == R.xml.preferences_locale) {
            return getString(R.string.pref_category_language);
        } else if (res == R.xml.preferences_search) {
            return getString(R.string.pref_category_search);
        } else if (res == R.xml.preferences_privacy) {
            return getString(R.string.pref_category_privacy_short);
        } else if (res == R.xml.preferences_accessibility) {
            return getString(R.string.pref_category_accessibility);
        } else if (res == R.xml.preferences_notifications) {
            return getString(R.string.pref_category_notifications);
        } else if (res == R.xml.preferences_advanced) {
            return getString(R.string.pref_category_advanced);
        } else if (res == R.xml.preferences_vendor) {
            return getString(R.string.pref_category_vendor);
        }

        return null;
    }

    /**
     * Return the header id for this preference fragment. This allows
     * us to select the correct header when launching a preference
     * screen directly.
     *
     * We only return titles for the preference screens that are
     * launched directly.
     */
    private int getHeader() {
        final int res = getResource();
        if (res == R.xml.preferences) {
            return R.id.pref_header_general;
        }

        // We can launch this category from the Data Reporting notification.
        if (res == R.xml.preferences_privacy) {
            return R.id.pref_header_privacy;
        }

        // We can launch this category from the the magnifying glass in the quick search bar.
        if (res == R.xml.preferences_search) {
            return R.id.pref_header_search;
        }

        if (res == R.xml.preferences_notifications) {
            return R.id.pref_header_notifications;
        }

        return HEADER_ID_UNDEFINED;
    }

    private void updateParentTitle() {
        final String newTitle = getTitle();
        if (newTitle == null) {
            Log.d(LOGTAG, "No new title to show.");
            return;
        }

        final GeckoPreferences activity = (GeckoPreferences) getActivity();
        if (activity.isMultiPane()) {
            activity.trySwitchToHeader(getHeader());
            return;
        }
        Log.v(LOGTAG, "Setting activity title to " + newTitle);
        activity.setTitle(newTitle);
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        accountLoaderCallbacks = new AccountLoaderCallbacks();
        getLoaderManager().initLoader(ACCOUNT_LOADER_ID, null, accountLoaderCallbacks);
    }

    @Override
    public void onResume() {
        // This is a little delicate. Ensure that you do nothing prior to
        // super.onResume that you wouldn't do in onCreate.
        // This will also set the title in the parent activity's ActionBar to the title of the current PreferenceScreen
        applyLocale(Locale.getDefault());
        super.onResume();

        // Force reload as the account may have been deleted while the app was in background.
        getLoaderManager().restartLoader(ACCOUNT_LOADER_ID, null, accountLoaderCallbacks);
    }

    private void applyLocale(final Locale currentLocale) {
        final Context context = getActivity().getApplicationContext();

        BrowserLocaleManager.getInstance().updateConfiguration(context, currentLocale);

        if (!currentLocale.equals(lastLocale)) {
            // Locales differ. Let's redisplay.
            Log.d(LOGTAG, "Locale changed: " + currentLocale);
            this.lastLocale = currentLocale;

            // Rebuild the list to reflect the current locale.
            getPreferenceScreen().removeAll();
            addPreferencesFromResource(getResource());
        }

        // Fix the parent title regardless.
        updateParentTitle();
    }

    /*
     * Get the resource from Fragment arguments and return it.
     *
     * If no resource can be found, return the resource id of the default preference screen.
     */
    private int getResource() {
        int resid = 0;

        final String resourceName = getArguments().getString("resource");
        final Activity activity = getActivity();

        if (resourceName != null) {
            // Fetch resource id by resource name.
            final Resources resources = activity.getResources();
            final String packageName = activity.getPackageName();
            resid = resources.getIdentifier(resourceName, "xml", packageName);
        }

        if (resid == 0) {
            // The resource was invalid. Use the default resource.
            Log.e(LOGTAG, "Failed to find resource: " + resourceName + ". Displaying default settings.");

            boolean isMultiPane = ((GeckoPreferences) activity).isMultiPane();
            resid = isMultiPane ? R.xml.preferences_general_tablet : R.xml.preferences;
        }

        return resid;
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        super.onCreateOptionsMenu(menu, inflater);
        inflater.inflate(R.menu.preferences_search_menu, menu);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (mPrefsRequest != null) {
            PrefsHelper.removeObserver(mPrefsRequest);
            mPrefsRequest = null;
        }

        final int res = getResource();
        if (res == R.xml.preferences) {
            Telemetry.stopUISession(TelemetryContract.Session.SETTINGS);
        }

        GeckoApplication.watchReference(getActivity(), this);
    }

    private class AccountLoaderCallbacks implements LoaderManager.LoaderCallbacks<Account> {
        @Override
        public Loader<Account> onCreateLoader(int id, Bundle args) {
            return new AccountLoader(getActivity());
        }

        @Override
        public void onLoadFinished(Loader<Account> loader, Account account) {
            if (syncPreference == null) {
                return;
            }

            if (account == null) {
                syncPreference.update(null);
                return;
            }

            syncPreference.update(new AndroidFxAccount(getActivity(), account));
        }

        @Override
        public void onLoaderReset(Loader<Account> loader) {
            if (syncPreference != null) {
                syncPreference.update(null);
            }
        }
    }
}
