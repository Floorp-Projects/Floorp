/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import android.content.Context;
import android.preference.Preference;
import android.preference.PreferenceCategory;
import android.util.AttributeSet;
import android.util.Log;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.util.GeckoEventListener;

public class SearchPreferenceCategory extends PreferenceCategory implements GeckoEventListener {
    public static final String LOGTAG = "SearchPrefCategory";

    private SearchEnginePreference mDefaultEngineReference;

    // These seemingly redundant constructors are mandated by the Android system, else it fails to
    // inflate this object.

    public SearchPreferenceCategory(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    public SearchPreferenceCategory(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public SearchPreferenceCategory(Context context) {
        super(context);
    }

    @Override
    protected void onAttachedToActivity() {
        super.onAttachedToActivity();

        // Ensures default engine remains at top of list.
        setOrderingAsAdded(false);

        // Request list of search engines from Gecko.
        GeckoAppShell.registerEventListener("SearchEngines:Data", this);
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("SearchEngines:Get", null));
    }

    @Override
    public void handleMessage(String event, final JSONObject data) {
        if (event.equals("SearchEngines:Data")) {
            // Parse engines array from JSON. The first element in the array is the default engine.
            JSONArray engines;
            JSONObject defaultEngine;
            final String defaultEngineName;
            try {
                engines = data.getJSONArray("searchEngines");
                if (engines.length() == 0) {
                    return;
                }
                defaultEngine = engines.getJSONObject(0);
                defaultEngineName = defaultEngine.getString("name");
            } catch (JSONException e) {
                Log.e(LOGTAG, "Unable to decode search engine data from Gecko.", e);
                return;
            }

            // Create an element in this PreferenceCategory for each engine.
            for (int i = 0; i < engines.length(); i++) {
                try {
                    JSONObject engineJSON = engines.getJSONObject(i);
                    final String engineName = engineJSON.getString("name");

                    SearchEnginePreference enginePreference = new SearchEnginePreference(getContext(), this);
                    enginePreference.setSearchEngineFromJSON(engineJSON);
                    if (engineName.equals(defaultEngineName)) {
                        // We set this here, not in setSearchEngineFromJSON, because it allows us to
                        // keep a reference  to the default engine to use when the AlertDialog
                        // callbacks are used.
                        enginePreference.setIsDefaultEngine(true);
                        mDefaultEngineReference = enginePreference;
                    }

                    enginePreference.setOnPreferenceClickListener(new OnPreferenceClickListener() {
                        @Override
                        public boolean onPreferenceClick(Preference preference) {
                            SearchEnginePreference sPref = (SearchEnginePreference) preference;
                            // Display the configuration dialog associated with the tapped engine.
                            sPref.showDialog();
                            return true;
                        }
                    });

                    addPreference(enginePreference);
                } catch (JSONException e) {
                    Log.e(LOGTAG, "JSONException parsing engine at index " + i, e);
                }
            }
        }

        // We are no longer interested in this event from Gecko, as we do not request it again with
        // this instance.
        GeckoAppShell.unregisterEventListener("SearchEngines:Data", this);
    }

    /**
     * Set the default engine to any available engine. Used if the current default is removed or
     * disabled.
     */
    private void setFallbackDefaultEngine() {
        if (getPreferenceCount() > 0) {
            SearchEnginePreference aEngine = (SearchEnginePreference) getPreference(0);
            setDefault(aEngine);
        }
    }

    /**
     * Helper method to send a particular event string to Gecko with an associated engine name.
     * @param event The type of event to send.
     * @param engine The engine to which the event relates.
     */
    private void sendGeckoEngineEvent(String event, SearchEnginePreference engine) {
        JSONObject json = new JSONObject();
        try {
            json.put("engine", engine.getTitle());
        } catch (JSONException e) {
            Log.e(LOGTAG, "JSONException creating search engine configuration change message for Gecko.", e);
            return;
        }
        GeckoAppShell.notifyGeckoOfEvent(GeckoEvent.createBroadcastEvent(event, json.toString()));
    }

    // Methods called by tapping items on the submenus for each search engine are below.

    /**
     * Removes the given engine from the set of available engines.
     * @param engine The engine to remove.
     */
    public void uninstall(SearchEnginePreference engine) {
        removePreference(engine);
        if (engine == mDefaultEngineReference) {
            // If they're deleting their default engine, get them a new default engine.
            setFallbackDefaultEngine();
        }

        sendGeckoEngineEvent("SearchEngines:Remove", engine);
    }

    /**
     * Sets the given engine as the current default engine.
     * @param engine The intended new default engine.
     */
    public void setDefault(SearchEnginePreference engine) {
        engine.setIsDefaultEngine(true);
        mDefaultEngineReference.setIsDefaultEngine(false);
        mDefaultEngineReference = engine;

        sendGeckoEngineEvent("SearchEngines:SetDefault", engine);
    }
}
