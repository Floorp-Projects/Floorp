/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.GeckoLayerClient;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.mozglue.GeckoLoader;
import org.mozilla.gecko.sqlite.SQLiteBridge;
import org.mozilla.gecko.util.GeckoEventListener;

import android.app.Activity;
import android.database.Cursor;
import android.view.View;

import java.nio.IntBuffer;

public class RobocopAPI {
    private final GeckoApp mGeckoApp;

    public RobocopAPI(Activity activity) {
        mGeckoApp = (GeckoApp)activity;
    }

    public void registerEventListener(String event, GeckoEventListener listener) {
        GeckoAppShell.registerEventListener(event, listener);
    }

    public void unregisterEventListener(String event, GeckoEventListener listener) {
        GeckoAppShell.unregisterEventListener(event, listener);
    }

    public void broadcastEvent(String subject, String data) {
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent(subject, data));
    }

    public void preferencesGetEvent(int requestId, String[] prefNames) {
        GeckoAppShell.sendEventToGecko(GeckoEvent.createPreferencesGetEvent(requestId, prefNames));
    }

    public void preferencesObserveEvent(int requestId, String[] prefNames) {
        GeckoAppShell.sendEventToGecko(GeckoEvent.createPreferencesObserveEvent(requestId, prefNames));
    }

    public void setDrawListener(GeckoLayerClient.DrawListener listener) {
        GeckoAppShell.getLayerView().getLayerClient().setDrawListener(listener);
    }

    public Cursor querySql(String dbPath, String query) {
        GeckoLoader.loadSQLiteLibs(mGeckoApp, mGeckoApp.getApplication().getPackageResourcePath());
        return new SQLiteBridge(dbPath).rawQuery(query, null);
    }

    public IntBuffer getViewPixels(View view) {
        return ((LayerView)view).getPixels();
    }
}
