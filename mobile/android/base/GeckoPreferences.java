/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Pakhotin <alexp@mozilla.com>
 *   Brian Nicholson <bnicholson@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko;

import java.util.ArrayList;

import android.os.Build;
import android.os.Bundle;
import android.content.res.Resources;
import android.content.Context;
import android.preference.*;
import android.preference.Preference.*;
import android.util.Log;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

public class GeckoPreferences
    extends PreferenceActivity
    implements OnPreferenceChangeListener, GeckoEventListener
{
    private static final String LOGTAG = "GeckoPreferences";

    private ArrayList<String> mPreferencesList = new ArrayList<String>();
    private PreferenceScreen mPreferenceScreen;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (Build.VERSION.SDK_INT >= 11)
            new GeckoActionBar().setDisplayHomeAsUpEnabled(this, true);

        addPreferencesFromResource(R.xml.preferences);
        mPreferenceScreen = getPreferenceScreen();
        GeckoAppShell.registerGeckoEventListener("Preferences:Data", this);
        initGroups(mPreferenceScreen);
        initValues();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        GeckoAppShell.unregisterGeckoEventListener("Preferences:Data", this);
    }

    public void handleMessage(String event, JSONObject message) {
        try {
            if (event.equals("Preferences:Data")) {
                JSONArray jsonPrefs = message.getJSONArray("preferences");
                refresh(jsonPrefs);
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "Exception handling message \"" + event + "\":", e);
        }
    }

    // Initialize preferences by sending the "Preferences:Get" command to Gecko
    private void initValues() {
        JSONArray jsonPrefs = new JSONArray(mPreferencesList);

        GeckoEvent event = new GeckoEvent("Preferences:Get", jsonPrefs.toString());
        GeckoAppShell.sendEventToGecko(event);
    }

    private void initGroups(PreferenceGroup preferences) {
        final int count = preferences.getPreferenceCount();
        for (int i = 0; i < count; i++) {
            Preference pref = preferences.getPreference(i);
            if (pref instanceof PreferenceGroup)
                initGroups((PreferenceGroup)pref);
            else {
                pref.setOnPreferenceChangeListener(this);
                mPreferencesList.add(pref.getKey());
            }
        }
    }
    
    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case android.R.id.home:
                finish();
                return true;
        }

        return super.onOptionsItemSelected(item);
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        String prefName = preference.getKey();
        setPreference(prefName, newValue);
        if (preference instanceof ListPreference)
            ((ListPreference)preference).setSummary((String)newValue);
        return true;
    }

    private void refresh(JSONArray jsonPrefs) {
        // enable all preferences once we have them from gecko
        GeckoAppShell.getMainHandler().post(new Runnable() {
            public void run() {
                mPreferenceScreen.setEnabled(true);
            }
        });

        try {
            if (mPreferenceScreen == null)
                return;

            // set the current page URL for the "Home page" preference
            final String[] homepageValues = getResources().getStringArray(R.array.pref_homepage_values);
            final Preference homepagePref = mPreferenceScreen.findPreference("browser.startup.homepage");
            GeckoAppShell.getMainHandler().post(new Runnable() {
                public void run() {
                    Tab tab = Tabs.getInstance().getSelectedTab();
                    homepageValues[2] = tab.getURL();
                    ((ListPreference)homepagePref).setEntryValues(homepageValues);
                }
            });

            final int length = jsonPrefs.length();
            for (int i = 0; i < length; i++) {
                JSONObject jPref = jsonPrefs.getJSONObject(i);
                final String prefName = jPref.getString("name");
                final String prefType = jPref.getString("type");
                final Preference pref = mPreferenceScreen.findPreference(prefName);

                if (prefName.equals("browser.startup.homepage")) {
                    final String value = jPref.getString("value");
                    GeckoAppShell.getMainHandler().post(new Runnable() {
                        public void run() {
                            pref.setSummary(value);
                        }
                    });
                }

                if (pref instanceof CheckBoxPreference && "bool".equals(prefType)) {
                    final boolean value = jPref.getBoolean("value");
                    GeckoAppShell.getMainHandler().post(new Runnable() {
                        public void run() {
                            if (((CheckBoxPreference)pref).isChecked() != value)
                                ((CheckBoxPreference)pref).setChecked(value);
                        }
                    });
                } else if (pref instanceof EditTextPreference && "string".equals(prefType)) {
                    final String value = jPref.getString("value");
                    GeckoAppShell.getMainHandler().post(new Runnable() {
                        public void run() {
                            ((EditTextPreference)pref).setText(value);
                        }
                    });
                } else if (pref instanceof ListPreference && "string".equals(prefType)) {
                    final String value = jPref.getString("value");
                    GeckoAppShell.getMainHandler().post(new Runnable() {
                        public void run() {
                            ((ListPreference)pref).setValue(value);
                        }
                    });
                }
            }
        } catch (JSONException e) {
            Log.e(LOGTAG, "Problem parsing preferences response: ", e);
        }
    }

    // send the Preferences:Set message to Gecko
    public static void setPreference(String pref, Object value) {
        try {
            JSONObject jsonPref = new JSONObject();
            jsonPref.put("name", pref);
            if (value instanceof Boolean) {
                jsonPref.put("type", "bool");
                jsonPref.put("value", ((Boolean)value).booleanValue());
            }
            else if (value instanceof Integer) {
                jsonPref.put("type", "int");
                jsonPref.put("value", ((Integer)value).intValue());
            }
            else {
                jsonPref.put("type", "string");
                jsonPref.put("value", String.valueOf(value));
            }

            GeckoEvent event = new GeckoEvent("Preferences:Set", jsonPref.toString());
            GeckoAppShell.sendEventToGecko(event);
        } catch (JSONException e) {
            Log.e(LOGTAG, "JSON exception: ", e);
        }
    }
}
