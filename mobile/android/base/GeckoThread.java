/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.mozglue.GeckoLoader;
import org.mozilla.gecko.mozglue.RobocopTarget;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.ThreadUtils;

import org.json.JSONObject;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;

import java.io.IOException;
import java.util.Locale;
import java.util.concurrent.atomic.AtomicReference;

public class GeckoThread extends Thread implements GeckoEventListener {
    private static final String LOGTAG = "GeckoThread";

    @RobocopTarget
    public enum LaunchState {
        Launching,
        WaitForDebugger,
        Launched,
        GeckoRunning,
        GeckoExiting,
        GeckoExited
    }

    private static AtomicReference<LaunchState> sLaunchState =
                                            new AtomicReference<LaunchState>(LaunchState.Launching);

    private static GeckoThread sGeckoThread;

    private final String mArgs;
    private final String mAction;
    private final String mUri;

    public static boolean ensureInit() {
        ThreadUtils.assertOnUiThread();
        if (isCreated())
            return false;
        sGeckoThread = new GeckoThread(sArgs, sAction, sUri);
        return true;
    }

    public static String sArgs;
    public static String sAction;
    public static String sUri;

    public static void setArgs(String args) {
        sArgs = args;
    }

    public static void setAction(String action) {
        sAction = action;
    }

    public static void setUri(String uri) {
        sUri = uri;
    }

    GeckoThread(String args, String action, String uri) {
        mArgs = args;
        mAction = action;
        mUri = uri;
        setName("Gecko");
        EventDispatcher.getInstance().registerGeckoThreadListener(this, "Gecko:Ready");
    }

    public static boolean isCreated() {
        return sGeckoThread != null;
    }

    public static void createAndStart() {
        if (ensureInit())
            sGeckoThread.start();
    }

    private String initGeckoEnvironment() {
        // At some point while loading the gecko libs our default locale gets set
        // so just save it to locale here and reset it as default after the join
        Locale locale = Locale.getDefault();

        if (locale.toString().equalsIgnoreCase("zh_hk")) {
            locale = Locale.TRADITIONAL_CHINESE;
            Locale.setDefault(locale);
        }

        Context context = GeckoAppShell.getContext();
        String resourcePath = "";
        Resources res  = null;
        String[] pluginDirs = null;
        try {
            pluginDirs = GeckoAppShell.getPluginDirectories();
        } catch (Exception e) {
            Log.w(LOGTAG, "Caught exception getting plugin dirs.", e);
        }

        resourcePath = context.getPackageResourcePath();
        res = context.getResources();
        GeckoLoader.setupGeckoEnvironment(context, pluginDirs, context.getFilesDir().getPath());

        GeckoLoader.loadSQLiteLibs(context, resourcePath);
        GeckoLoader.loadNSSLibs(context, resourcePath);
        GeckoLoader.loadGeckoLibs(context, resourcePath);
        GeckoJavaSampler.setLibsLoaded();

        Locale.setDefault(locale);

        Configuration config = res.getConfiguration();
        config.locale = locale;
        res.updateConfiguration(config, null);

        return resourcePath;
    }

    private String getTypeFromAction(String action) {
        if (action != null && action.startsWith(GeckoApp.ACTION_WEBAPP_PREFIX)) {
            return "-webapp";
        }
        if (GeckoApp.ACTION_HOMESCREEN_SHORTCUT.equals(action)) {
            return "-bookmark";
        }
        return null;
    }

    private String addCustomProfileArg(String args) {
        String profile = "";
        String guest = "";
        if (GeckoAppShell.getGeckoInterface() != null) {
            if (GeckoAppShell.getGeckoInterface().getProfile().inGuestMode()) {
                try {
                    profile = " -profile " + GeckoAppShell.getGeckoInterface().getProfile().getDir().getCanonicalPath();
                } catch (IOException ioe) { Log.e(LOGTAG, "error getting guest profile path", ioe); }

                if (args == null || !args.contains(BrowserApp.GUEST_BROWSING_ARG)) {
                    guest = " " + BrowserApp.GUEST_BROWSING_ARG;
                }
            } else if (!GeckoProfile.sIsUsingCustomProfile) {
                // If nothing was passed in in the intent, force Gecko to use the default profile for
                // for this activity
                profile = " -P " + GeckoAppShell.getGeckoInterface().getProfile().getName();
            }
        }

        return (args != null ? args : "") + profile + guest;
    }

    @Override
    public void run() {
        Looper.prepare();
        ThreadUtils.sGeckoThread = this;
        ThreadUtils.sGeckoHandler = new Handler();
        ThreadUtils.sGeckoQueue = Looper.myQueue();

        String path = initGeckoEnvironment();

        Log.w(LOGTAG, "zerdatime " + SystemClock.uptimeMillis() + " - runGecko");

        String args = addCustomProfileArg(mArgs);
        String type = getTypeFromAction(mAction);

        if (!AppConstants.MOZILLA_OFFICIAL) {
            Log.i(LOGTAG, "RunGecko - args = " + args);
        }
        // and then fire us up
        GeckoAppShell.runGecko(path, args, mUri, type);
    }

    @Override
    public void handleMessage(String event, JSONObject message) {
        if ("Gecko:Ready".equals(event)) {
            EventDispatcher.getInstance().unregisterGeckoThreadListener(this, event);
            setLaunchState(LaunchState.GeckoRunning);
            GeckoAppShell.sendPendingEventsToGecko();
        }
    }

    @RobocopTarget
    public static boolean checkLaunchState(LaunchState checkState) {
        return sLaunchState.get() == checkState;
    }

    static void setLaunchState(LaunchState setState) {
        sLaunchState.set(setState);
    }

    /**
     * Set the launch state to <code>setState</code> and return true if the current launch
     * state is <code>checkState</code>; otherwise do nothing and return false.
     */
    static boolean checkAndSetLaunchState(LaunchState checkState, LaunchState setState) {
        return sLaunchState.compareAndSet(checkState, setState);
    }
}
