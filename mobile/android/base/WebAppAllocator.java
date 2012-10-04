/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.util.Base64;
import android.util.Log;

import java.io.FileOutputStream;

import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.util.GeckoBackgroundThread;

public class WebAppAllocator {
    private final String LOGTAG = "GeckoWebAppAllocator";
    // The number of WebApp# and WEBAPP# activites/apps/intents
    private final static int MAX_WEB_APPS = 100;

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

        // The marketplaceApp will run in this same process, but has a different context
        // Rather than just failing, we want to create a new Allocator instead
        if (cx != sContext) {
            sInstance = null;
            sContext = (GeckoApp) cx;
            sInstance = new WebAppAllocator(cx);
        }

        return sInstance;
    }

    SharedPreferences mPrefs;

    protected WebAppAllocator(Context context) {
        mPrefs = context.getSharedPreferences("webapps", Context.MODE_PRIVATE | Context.MODE_MULTI_PROCESS);
    }

    public static String appKey(int index) {
        return "app" + index;
    }

    static public String iconKey(int index) {
        return "icon" + index;
    }

    public synchronized int findAndAllocateIndex(String app, String name, String aIconData) {
        byte[] raw = Base64.decode(aIconData.substring(22), Base64.DEFAULT);
        Bitmap bitmap = BitmapFactory.decodeByteArray(raw, 0, raw.length);
        return findAndAllocateIndex(app, name, bitmap);
    }

    public synchronized int findAndAllocateIndex(final String app, final String name, final Bitmap aIcon) {
        int index = getIndexForApp(app);
        if (index != -1)
            return index;

        for (int i = 0; i < MAX_WEB_APPS; ++i) {
            if (!mPrefs.contains(appKey(i))) {
                // found unused index i
                final int foundIndex = i;
                GeckoBackgroundThread.getHandler().post(new Runnable() {
                    public void run() {
                        int color = 0;
                        try {
                            color = BitmapUtils.getDominantColor(aIcon);
                        } catch (Exception e) {
                            e.printStackTrace();
                        }

                        mPrefs.edit()
                            .putString(appKey(foundIndex), app)
                            .putInt(iconKey(foundIndex), color)
                            .commit();
                    }
                });
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

    public synchronized void releaseIndex(final int index) {
        GeckoBackgroundThread.getHandler().post(new Runnable() {
            public void run() {
                mPrefs.edit()
                    .remove(appKey(index))
                    .remove(iconKey(index))
                    .commit();
            }
        });
    }
}
