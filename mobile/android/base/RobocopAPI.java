/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.GeckoLayerClient;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.gfx.PanningPerfAPI;
import org.mozilla.gecko.mozglue.GeckoLoader;
import org.mozilla.gecko.mozglue.RobocopTarget;
import org.mozilla.gecko.sqlite.SQLiteBridge;
import org.mozilla.gecko.util.GeckoEventListener;

import android.app.Activity;
import android.database.Cursor;
import android.view.View;

import java.nio.IntBuffer;
import java.util.List;

/**
 * Class to provide wrapper methods around methods wanted by Robocop.
 *
 * This class provides fixed entry points into code that is liable to be optimised by Proguard without
 * needing to prevent Proguard from optimising the wrapped methods.
 * Wrapping in this way still slightly hinders Proguard's ability to optimise.
 *
 * If you find yourself wanting to add a method to this class - proceed with caution. If you're writing
 * a test that's not about manipulating the UI, you might be better off using JUnit (Or similar)
 * instead of Robocop.
 *
 * Alternatively, you might be able to get what you want by reflecting on a method annotated for the
 * benefit of the C++ wrapper generator - these methods are sure to not disappear at compile-time.
 * 
 * Finally, you might be able to get what you want via Reflection on Android's libraries. Those are
 * also not prone to vanishing at compile-time, but doing this might substantially complicate your
 * work, ultimately not proving worth the extra effort to avoid making a slight mess here.
 */
@RobocopTarget
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

    public void preferencesRemoveObserversEvent(int requestId) {
        GeckoAppShell.sendEventToGecko(GeckoEvent.createPreferencesRemoveObserversEvent(requestId));
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

    // PanningPerfAPI.
    public static void startFrameTimeRecording() {
        PanningPerfAPI.startFrameTimeRecording();
    }

    public static List<Long> stopFrameTimeRecording() {
        return PanningPerfAPI.stopFrameTimeRecording();
    }

    public static void startCheckerboardRecording() {
        PanningPerfAPI.startCheckerboardRecording();
    }

    public static List<Float> stopCheckerboardRecording() {
        return PanningPerfAPI.stopCheckerboardRecording();
    }
}
