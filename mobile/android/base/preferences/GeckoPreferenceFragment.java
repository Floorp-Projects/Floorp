/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import java.util.Locale;

import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.BrowserLocaleManager;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.LocaleManager;
import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.TelemetryContract.Method;

import android.app.ActionBar;
import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.os.Bundle;
import android.preference.PreferenceActivity;
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
    private int mPrefsRequestId;
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
        mPrefsRequestId = ((GeckoPreferences)getActivity()).setupPreferences(screen);
    }

    /**
     * Return the title to use for this preference fragment. This allows
     * for us to redisplay this fragment in a different locale.
     *
     * We only return titles for the preference screens that are in the
     * flow for selecting a locale, and thus might need to be redisplayed.
     *
     * This method sets the title that you see on non-multi-pane devices.
     */
    private String getTitle() {
        final int res = getResource();
        if (res == R.xml.preferences_locale) {
            return getString(R.string.pref_category_language);
        }

        if (res == R.xml.preferences) {
            return getString(R.string.settings_title);
        }

        // We need this because we can launch straight into this category
        // from the Data Reporting notification.
        if (res == R.xml.preferences_vendor) {
            return getString(R.string.pref_category_vendor);
        }

        // from the Awesomescreen with the magnifying glass.
        if (res == R.xml.preferences_search) {
            return getString(R.string.pref_category_search);
        }

        return null;
    }

    private void updateTitle() {
        final String newTitle = getTitle();
        if (newTitle == null) {
            Log.d(LOGTAG, "No new title to show.");
            return;
        }

        final PreferenceActivity activity = (PreferenceActivity) getActivity();
        if (Versions.feature11Plus && activity.isMultiPane()) {
            // In a multi-pane activity, the title is "Settings", and the action
            // bar is along the top of the screen. We don't want to change those.
            activity.showBreadCrumbs(newTitle, newTitle);
            return;
        }

        Log.v(LOGTAG, "Setting activity title to " + newTitle);
        activity.setTitle(newTitle);

        if (Versions.feature14Plus) {
            final ActionBar actionBar = activity.getActionBar();
            if (actionBar != null) {
                actionBar.setTitle(newTitle);
            }
        }
    }

    @Override
    public void onResume() {
        // This is a little delicate. Ensure that you do nothing prior to
        // super.onResume that you wouldn't do in onCreate.
        applyLocale(Locale.getDefault());
        super.onResume();
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
        updateTitle();
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

            boolean isMultiPane = Versions.feature11Plus &&
                                  ((PreferenceActivity) activity).isMultiPane();
            resid = isMultiPane ? R.xml.preferences_customize_tablet : R.xml.preferences;
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
        if (mPrefsRequestId > 0) {
            PrefsHelper.removeObserver(mPrefsRequestId);
        }

        final int res = getResource();
        if (res == R.xml.preferences) {
            Telemetry.stopUISession(TelemetryContract.Session.SETTINGS);
        }
    }
}
