/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.util.Log;
import android.content.Context;
import android.content.SharedPreferences;

import org.json.JSONObject;
import org.json.JSONException;

public class WebAppAllocator {
    // The number of WebApp# and WEBAPP# activites/apps/intents
    private final static int MAX_WEB_APPS = 10;

    protected static GeckoApp sContext = null;
    protected static WebAppAllocator sInstance = null;
    public static WebAppAllocator getInstance() {
        return getInstance(GeckoApp.mAppContext);
    }

    public static synchronized WebAppAllocator getInstance(Context cx) {
        if (sInstance == null) {
            if (!(cx instanceof GeckoApp))
                throw new RuntimeException("Context needs to be a GeckoApp");
                
            sContext = (GeckoApp) cx;
            sInstance = new WebAppAllocator(cx);
        }

        if (cx != sContext)
            throw new RuntimeException("Tried to get WebAppAllocator instance for different context than it was created for");

        return sInstance;
    }

    SharedPreferences mPrefs;

    protected WebAppAllocator(Context context) {
        mPrefs = context.getSharedPreferences("webapps", Context.MODE_PRIVATE | Context.MODE_MULTI_PROCESS);
    }

    static String appKey(int index) {
        return "app" + index;
    }

    public synchronized int findAndAllocateIndex(String app) {
        int index = getIndexForApp(app);
        if (index != -1)
            return index;

        for (int i = 0; i < MAX_WEB_APPS; ++i) {
            if (!mPrefs.contains(appKey(i))) {
                // found unused index i
                mPrefs.edit()
                    .putString(appKey(i), app)
                    .apply();
                return i;
            }
        }

        // no more apps!
        return -1;
    }

    public synchronized int getIndexForApp(String app) {
        for (int i = 0; i < MAX_WEB_APPS; ++i) {
            if (mPrefs.getString(appKey(i), "").equals(app)) {
                return i;
            }
        }

        return -1;
    }

    public synchronized String getAppForIndex(int index) {
            return mPrefs.getString(appKey(index), null);
    }

    public synchronized int releaseIndexForApp(String app) {
        int index = getIndexForApp(app);
        if (index == -1)
            return -1;

        releaseIndex(index);
        return index;
    }

    public synchronized void releaseIndex(int index) {
        mPrefs.edit()
            .remove(appKey(index))
            .apply();
    }
}
