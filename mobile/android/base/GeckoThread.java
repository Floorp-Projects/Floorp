/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.mozglue.GeckoLoader;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.ThreadUtils;

import org.json.JSONObject;

import android.content.Intent;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;
import android.app.Activity;


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

    private Intent mIntent;
    private final String mUri;

    GeckoThread(Intent intent, String uri) {
        mIntent = intent;
        mUri = uri;
        setName("Gecko");
        GeckoAppShell.getEventDispatcher().registerEventListener("Gecko:Ready", this);
    }

    private String initGeckoEnvironment() {
        // At some point while loading the gecko libs our default locale gets set
        // so just save it to locale here and reset it as default after the join
        Locale locale = Locale.getDefault();

        if (locale.toString().equalsIgnoreCase("zh_hk")) {
            locale = Locale.TRADITIONAL_CHINESE;
            Locale.setDefault(locale);
        }

        Context app = GeckoAppShell.getContext();
        String resourcePath = "";
        Resources res  = null;
        String[] pluginDirs = null;
        try {
            pluginDirs = GeckoAppShell.getPluginDirectories();
        } catch (Exception e) {
            Log.w(LOGTAG, "Caught exception getting plugin dirs.", e);
        }
        
        if (app instanceof Activity) {
            Activity activity = (Activity)app;
            resourcePath = activity.getApplication().getPackageResourcePath();
            res = activity.getBaseContext().getResources();
            GeckoLoader.setupGeckoEnvironment(activity, pluginDirs, GeckoProfile.get(app).getFilesDir().getPath());
        }
        GeckoLoader.loadSQLiteLibs(app, resourcePath);
        GeckoLoader.loadNSSLibs(app, resourcePath);
        GeckoLoader.loadGeckoLibs(app, resourcePath);
        GeckoJavaSampler.setLibsLoaded();

        Locale.setDefault(locale);

        Configuration config = res.getConfiguration();
        config.locale = locale;
        res.updateConfiguration(config, res.getDisplayMetrics());

        return resourcePath;
    }

    private String getTypeFromAction(String action) {
        if (action != null && action.startsWith(GeckoApp.ACTION_WEBAPP_PREFIX)) {
            return "-webapp";
        }
        if (GeckoApp.ACTION_BOOKMARK.equals(action)) {
            return "-bookmark";
        }
        return null;
    }

    private String addCustomProfileArg(String args) {
        String profile = GeckoAppShell.getGeckoInterface() == null || GeckoApp.sIsUsingCustomProfile ? "" : (" -P " + GeckoAppShell.getGeckoInterface().getProfile().getName());
        return (args != null ? args : "") + profile;
    }

    @Override
    public void run() {
        Looper.prepare();
        ThreadUtils.setGeckoThread(this, new Handler());

        String path = initGeckoEnvironment();

        Log.w(LOGTAG, "zerdatime " + SystemClock.uptimeMillis() + " - runGecko");

        String args = addCustomProfileArg(mIntent.getStringExtra("args"));
        String type = getTypeFromAction(mIntent.getAction());
        mIntent = null;

        // and then fire us up
        Log.i(LOGTAG, "RunGecko - args = " + args);
        GeckoAppShell.runGecko(path, args, mUri, type);
    }

    @Override
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
