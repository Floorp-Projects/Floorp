/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.webapp;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;

import java.util.ArrayList;

import android.util.Log;

public class WebAppAllocator {

    private final String LOGTAG = "GeckoWebAppAllocator";

    private static final String PREFIX_ORIGIN = "webapp-origin-";
    private static final String PREFIX_PACKAGE_NAME = "webapp-package-name-";

    // The number of WebApp# and WEBAPP# activites/apps/intents
    private final static int MAX_WEB_APPS = 100;

    protected static WebAppAllocator sInstance = null;
    public static WebAppAllocator getInstance() {
        return getInstance(GeckoAppShell.getContext());
    }

    public static synchronized WebAppAllocator getInstance(Context cx) {
        if (sInstance == null) {
            sInstance = new WebAppAllocator(cx);
        }

        return sInstance;
    }

    SharedPreferences mPrefs;

    protected WebAppAllocator(Context context) {
        mPrefs = context.getSharedPreferences("webapps", Context.MODE_PRIVATE | Context.MODE_MULTI_PROCESS);
    }

    private static String appKey(int index) {
        return PREFIX_PACKAGE_NAME + index;
    }

    public static String iconKey(int index) {
        return "web-app-color-" + index;
    }

    public static String originKey(int i) {
        return PREFIX_ORIGIN + i;
    }

    public ArrayList<String> getInstalledPackageNames() {
        ArrayList<String> installedPackages = new ArrayList<String>();

        for (int i = 0; i < MAX_WEB_APPS; ++i) {
            if (mPrefs.contains(appKey(i))) {
                installedPackages.add(mPrefs.getString(appKey(i), ""));
            }
        }
        return installedPackages;
    }

    public synchronized int findOrAllocatePackage(final String packageName) {
        int index = getIndexForApp(packageName);
        if (index != -1)
            return index;

        for (int i = 0; i < MAX_WEB_APPS; ++i) {
            if (!mPrefs.contains(appKey(i))) {
                // found unused index i
                putPackageName(i, packageName);
                return i;
            }
        }

        // no more apps!
        return -1;
    }

    public synchronized void putPackageName(final int index, final String packageName) {
        mPrefs.edit().putString(appKey(index), packageName).apply();
    }

    public void updateColor(int index, int color) {
        mPrefs.edit().putInt(iconKey(index), color).apply();
    }

    public synchronized int getIndexForApp(String packageName) {
        return findSlotForPrefix(PREFIX_PACKAGE_NAME, packageName);
    }

    public synchronized int getIndexForOrigin(String origin) {
        return findSlotForPrefix(PREFIX_ORIGIN, origin);
    }

    protected int findSlotForPrefix(String prefix, String value) {
        for (int i = 0; i < MAX_WEB_APPS; ++i) {
            if (mPrefs.getString(prefix + i, "").equals(value)) {
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
        mPrefs.edit()
              .remove(appKey(index))
              .remove(iconKey(index))
              .remove(originKey(index))
              .apply();
    }

    public void putOrigin(int index, String origin) {
        mPrefs.edit().putString(originKey(index), origin).apply();
    }

    public String getOrigin(int index) {
        return mPrefs.getString(originKey(index), null);
    }

    public int getColor(int index) {
        return mPrefs.getInt(iconKey(index), -1);
    }
}
