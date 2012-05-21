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

public class GeckoThread extends Thread {
    private static final String LOGTAG = "GeckoThread";

    Intent mIntent;
    String mUri;
    int mRestoreMode;

    GeckoThread(Intent intent, String uri, int restoreMode) {
        mIntent = intent;
        mUri = uri;
        mRestoreMode = restoreMode;

        setName("Gecko");
    }

    public void run() {
        final GeckoApp app = GeckoApp.mAppContext;
        File cacheFile = GeckoAppShell.getCacheDir(app);
        File libxulFile = new File(cacheFile, "libxul.so");

        if ((!libxulFile.exists() ||
             new File(app.getApplication().getPackageResourcePath()).lastModified() >= libxulFile.lastModified())) {
            File[] libs = cacheFile.listFiles(new FilenameFilter() {
                public boolean accept(File dir, String name) {
                    return name.endsWith(".so");
                }
            });
            if (libs != null) {
                for (int i = 0; i < libs.length; i++) {
                    libs[i].delete();
                }
            }
        }

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
        String type = GeckoApp.ACTION_WEBAPP.equals(action) ? "-webapp" :
                      GeckoApp.ACTION_BOOKMARK.equals(action) ? "-bookmark" :
                      null;

        // and then fire us up
        Log.i(LOGTAG, "RunGecko - URI = " + mUri);
        GeckoAppShell.runGecko(app.getApplication().getPackageResourcePath(),
                               mIntent.getStringExtra("args"),
                               mUri,
                               type,
                               mRestoreMode);
    }
}
