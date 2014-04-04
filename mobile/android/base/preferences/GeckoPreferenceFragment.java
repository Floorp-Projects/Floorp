/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import java.lang.reflect.Field;

import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.R;

import android.app.Activity;
import android.os.Bundle;
import android.preference.PreferenceActivity;
import android.preference.PreferenceFragment;
import android.preference.PreferenceScreen;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.ViewConfiguration;

/* A simple implementation of PreferenceFragment for large screen devices
 * This will strip category headers (so that they aren't shown to the user twice)
 * as well as initializing Gecko prefs when a fragment is shown.
*/
public class GeckoPreferenceFragment extends PreferenceFragment {

    private static final String LOGTAG = "GeckoPreferenceFragment";
    private int mPrefsRequestId = 0;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Write prefs to our custom GeckoSharedPrefs file.
        getPreferenceManager().setSharedPreferencesName(GeckoSharedPrefs.APP_PREFS_NAME);

        int res = getResource();

        // Display a menu for Search preferences.
        if (res == R.xml.preferences_search) {
            setHasOptionsMenu(true);
        }

        addPreferencesFromResource(res);

        PreferenceScreen screen = getPreferenceScreen();
        setPreferenceScreen(screen);
        mPrefsRequestId = ((GeckoPreferences)getActivity()).setupPreferences(screen);
    }

    /*
     * Get the resource from Fragment arguments and return it.
     *
     * If no resource can be found, return the resource id of the default preference screen.
     */
    private int getResource() {
        int resid = 0;

        String resourceName = getArguments().getString("resource");
        if (resourceName != null) {
            // Fetch resource id by resource name.
            resid = getActivity().getResources().getIdentifier(resourceName,
                                                             "xml",
                                                             getActivity().getPackageName());
        }

        if (resid == 0) {
            // The resource was invalid. Use the default resource.
            Log.e(LOGTAG, "Failed to find resource: " + resourceName + ". Displaying default settings.");

            boolean isMultiPane = ((PreferenceActivity) getActivity()).onIsMultiPane();
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
    }

    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);
        showOverflowMenu(activity);
    }

    /*
     * Force the overflow 3-dot menu to be displayed if it isn't already displayed.
     *
     * This is an ugly hack for 4.0+ Android devices that don't have a dedicated menu button
     * because Android does not provide a public API to display the ActionBar overflow menu.
     */
    private void showOverflowMenu(Activity activity) {
        try {
            ViewConfiguration config = ViewConfiguration.get(activity);
            Field menuOverflow = ViewConfiguration.class.getDeclaredField("sHasPermanentMenuKey");
            if (menuOverflow != null) {
                menuOverflow.setAccessible(true);
                menuOverflow.setBoolean(config, false);
            }
        } catch (Exception e) {
            Log.d(LOGTAG, "Failed to force overflow menu, ignoring.");
        }
    }
}
