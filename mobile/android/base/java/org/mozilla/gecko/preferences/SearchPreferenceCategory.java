/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import android.content.Context;
import android.support.annotation.Nullable;
import android.support.v7.util.DiffUtil;
import android.support.v7.util.ListUpdateCallback;
import android.util.AttributeSet;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.TelemetryContract.Method;
import org.mozilla.gecko.search.SearchEngineDiffCallback;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

import java.util.ArrayList;
import java.util.List;

public class SearchPreferenceCategory extends CustomListCategory implements BundleEventListener, ListUpdateCallback {
    public static final String LOGTAG = "SearchPrefCategory";
    private List<SearchEnginePreference> enginesList = new ArrayList<>();

    public SearchPreferenceCategory(Context context) {
        super(context);
    }

    public SearchPreferenceCategory(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public SearchPreferenceCategory(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    protected void onAttachedToActivity() {
        super.onAttachedToActivity();

        // Register for SearchEngines messages and request list of search engines from Gecko.
        EventDispatcher.getInstance().registerUiThreadListener(this, "SearchEngines:Data");
        EventDispatcher.getInstance().dispatch("SearchEngines:GetVisible", null);
    }

    @Override
    protected void onPrepareForRemoval() {
        super.onPrepareForRemoval();

        EventDispatcher.getInstance().unregisterUiThreadListener(this, "SearchEngines:Data");
    }

    @Override
    public void setDefault(CustomListPreference item) {
        super.setDefault(item);

        sendGeckoEngineEvent("SearchEngines:SetDefault", item.getTitle().toString());

        final String identifier = ((SearchEnginePreference) item).getIdentifier();
        Telemetry.sendUIEvent(TelemetryContract.Event.SEARCH_SET_DEFAULT, Method.DIALOG, identifier);
    }

    @Override
    public void uninstall(CustomListPreference item) {
        super.uninstall(item);

        sendGeckoEngineEvent("SearchEngines:Remove", item.getTitle().toString());

        final String identifier = ((SearchEnginePreference) item).getIdentifier();
        Telemetry.sendUIEvent(TelemetryContract.Event.SEARCH_REMOVE, Method.DIALOG, identifier);
    }

    @Override // BundleEventListener
    public void handleMessage(final String event, final GeckoBundle data,
                              final EventCallback callback) {
        if (event.equals("SearchEngines:Data")) {
            // Parse engines array from bundle into ArrayList.
            GeckoBundle[] engines = data.getBundleArray("searchEngines");
            List<SearchEnginePreference> newEngineList = new ArrayList<>();

            for (GeckoBundle engine: engines) {
                SearchEnginePreference enginePreference =
                        new SearchEnginePreference(getContext(), this);
                enginePreference.setSearchEngineFromBundle(engine);
                enginePreference.setOnPreferenceClickListener(preference -> {
                    SearchEnginePreference sPref = (SearchEnginePreference) preference;
                    // Display the configuration dialog associated with the tapped engine.
                    sPref.showDialog();
                    return true;
                });

                newEngineList.add(enginePreference);
            }

            DiffUtil.DiffResult diffResult = DiffUtil.calculateDiff(new SearchEngineDiffCallback(enginesList, newEngineList));
            enginesList = newEngineList;
            diffResult.dispatchUpdatesTo(this);
        }
    }

    @Override
    public void onInserted(int pos, int count) {
        for (int ix = pos; ix < (pos + count); ++ix) {

            addPreference(enginesList.get(ix));

            // The first element in the array is the default engine.
            // We set this here, not in setSearchEngineFromBundle, because it allows us to
            // keep a reference  to the default engine to use when the AlertDialog
            // callbacks are used.
            if (ix == 0) {
                enginesList.get(ix).setIsDefault(true);
                mDefaultReference = enginesList.get(ix);
            }
        }
    }


    // The onRemoved, onMoved and onChanged entries are not used here as the list is never reordered
    // by the clicking of the completion checkbox preference which activates this code.
    @Override
    public void onRemoved(int pos, int count) {

    }

    @Override
    public void onMoved(int i, int i1) {

    }

    @Override
    public void onChanged(int i, int i1, @Nullable Object o) {

    }

    /**
     * Helper method to send a particular event string to Gecko with an associated engine name.
     * @param event The type of event to send.
     * @param engine The engine to which the event relates.
     */
    private void sendGeckoEngineEvent(final String event, final String engineName) {
        final GeckoBundle data = new GeckoBundle(1);
        data.putString("engine", engineName);
        EventDispatcher.getInstance().dispatch(event, data);
    }
}
