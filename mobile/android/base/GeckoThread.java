/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Intent;
import android.content.res.Resources;
import android.content.res.Configuration;
import android.os.SystemClock;
import android.util.Log;
import android.widget.AbsoluteLayout;

import java.io.File;
import java.io.FilenameFilter;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.Date;
import java.util.Locale;
import java.util.concurrent.CountDownLatch;

public class GeckoThread extends Thread {
    private static final String LOGTAG = "GeckoThread";

    Intent mIntent;
    String mUri;
    int mRestoreMode;
    CountDownLatch mStartSignal;

    GeckoThread() {
        mStartSignal = new CountDownLatch(1);
        setName("Gecko");
    }

    public void init(Intent intent, String uri, int restoreMode) {
        mIntent = intent;
        mUri = uri;
        mRestoreMode = restoreMode;
    }

    public void reallyStart() {
        mStartSignal.countDown();
        if (getState() == Thread.State.NEW)
            start();
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

        try {
            mStartSignal.await();
        } catch (Exception e) { }

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

        // if this isn't the default BrowserApp, send the apps default profile to gecko
        if (!(app instanceof BrowserApp)) {
            String profile = app.getDefaultProfileName();
            args = (args != null ? args : "") + "-P " + profile;
        }

        // and then fire us up
        Log.i(LOGTAG, "RunGecko - URI = " + mUri + " args = " + args);
        GeckoAppShell.runGecko(app.getApplication().getPackageResourcePath(),
                               args,
                               mUri,
                               type,
                               mRestoreMode);
    }
}
