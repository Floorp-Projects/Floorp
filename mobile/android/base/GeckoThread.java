/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.GfxInfoThread;
import org.mozilla.gecko.util.GeckoEventListener;

import org.json.JSONObject;

import android.content.Intent;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.os.SystemClock;
import android.util.Log;

import java.util.Locale;

public class GeckoThread extends Thread implements GeckoEventListener {
    private static final String LOGTAG = "GeckoThread";

    public enum LaunchState {
        Launching,
        WaitForDebugger,
        Launched,
        GeckoRunning,
        GeckoExiting
    };

    private static LaunchState sLaunchState = LaunchState.Launching;

    private final Intent mIntent;
    private final String mUri;

    GeckoThread(Intent intent, String uri) {
        mIntent = intent;
        mUri = uri;
        setName("Gecko");
        GeckoAppShell.getEventDispatcher().registerEventListener("Gecko:Ready", this);
    }

    public void run() {

        // Here we start the GfxInfo thread, which will query OpenGL
        // system information for Gecko. This must be done early enough that the data will be
        // ready by the time it's needed to initialize the LayerManager (it takes about 100 ms
        // to obtain). Doing it here seems to have no negative effect on startup time. See bug 766251.
        // Starting the GfxInfoThread here from the GeckoThread, ensures that
        // the Gecko thread is started first, adding some determinism there.
        GeckoAppShell.sGfxInfoThread = new GfxInfoThread();
        GeckoAppShell.sGfxInfoThread.start();

        final GeckoApp app = GeckoApp.mAppContext;

        // At some point while loading the gecko libs our default locale gets set
        // so just save it to locale here and reset it as default after the join
        Locale locale = Locale.getDefault();

        String resourcePath = app.getApplication().getPackageResourcePath();
        GeckoAppShell.setupGeckoEnvironment(app);
        GeckoAppShell.loadSQLiteLibs(app, resourcePath);
        GeckoAppShell.loadNSSLibs(app, resourcePath);
        GeckoAppShell.loadGeckoLibs(resourcePath);

        Locale.setDefault(locale);
        Resources res = app.getBaseContext().getResources();
        Configuration config = res.getConfiguration();
        config.locale = locale;
        res.updateConfiguration(config, res.getDisplayMetrics());

        Log.w(LOGTAG, "zerdatime " + SystemClock.uptimeMillis() + " - runGecko");

        // find the right intent type
        final String action = mIntent.getAction();
        String type = null;

        if (action != null && action.startsWith(GeckoApp.ACTION_WEBAPP_PREFIX))
            type = "-webapp";
        else if (GeckoApp.ACTION_BOOKMARK.equals(action))
            type = "-bookmark";

        String args = mIntent.getStringExtra("args");

        String profile = GeckoApp.sIsUsingCustomProfile ? "" : (" -P " + app.getProfile().getName());
        args = (args != null ? args : "") + profile;

        // and then fire us up
        Log.i(LOGTAG, "RunGecko - args = " + args);
        GeckoAppShell.runGecko(app.getApplication().getPackageResourcePath(),
                               args,
                               mUri,
                               type);
    }

    public void handleMessage(String event, JSONObject message) {
        if ("Gecko:Ready".equals(event)) {
            GeckoAppShell.getEventDispatcher().unregisterEventListener(event, this);
            setLaunchState(LaunchState.GeckoRunning);
            GeckoAppShell.sendPendingEventsToGecko();
        }
    }

    public static boolean checkLaunchState(LaunchState checkState) {
        synchronized (sLaunchState) {
            return sLaunchState == checkState;
        }
    }

    static void setLaunchState(LaunchState setState) {
        synchronized (sLaunchState) {
            sLaunchState = setState;
        }
    }

    /**
     * Set the launch state to <code>setState</code> and return true if the current launch
     * state is <code>checkState</code>; otherwise do nothing and return false.
     */
    static boolean checkAndSetLaunchState(LaunchState checkState, LaunchState setState) {
        synchronized (sLaunchState) {
            if (sLaunchState != checkState)
                return false;
            sLaunchState = setState;
            return true;
        }
    }
}
