/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.webapp;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.text.TextUtils;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;
import org.json.JSONArray;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;

import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.ArrayList;

public class UninstallListener extends BroadcastReceiver {

    private static String LOGTAG = "GeckoUninstallListener";

    @Override
    public void onReceive(Context context, Intent intent) {
        String packageName = intent.getData().getSchemeSpecificPart();

        if (TextUtils.isEmpty(packageName)) {
            Log.i(LOGTAG, "No package name defined in intent");
            return;
        }

        WebAppAllocator allocator = WebAppAllocator.getInstance(context);
        ArrayList<String> installedPackages = allocator.getInstalledPackageNames();

        if (installedPackages.contains(packageName)) {
            JSONObject message = new JSONObject();
            JSONArray packageNames = new JSONArray();
            try {
                packageNames.put(packageName);
                message.put("packages", packageNames);
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Webapps:AutoUninstall", message.toString()));
            } catch (JSONException e) {
                Log.e(LOGTAG, "JSON EXCEPTION " + e);
            }
        }
    }

    public static void initUninstallPackageScan(Context context) {
        // get list of packages we think are installed
        WebAppAllocator allocator = WebAppAllocator.getInstance(context);
        ArrayList<String> fennecPackages = allocator.getInstalledPackageNames();
        ArrayList<String> uninstalledPackages = new ArrayList<String>();

        final PackageManager pm = context.getPackageManager();
        //get a list of installed apps on device
        List<ApplicationInfo> packages = pm.getInstalledApplications(PackageManager.GET_META_DATA);
        Set<String> allInstalledPackages = new HashSet<String>();

        for (ApplicationInfo packageInfo : packages) {
            //Log.i(LOGTAG, "Android package: " + packageInfo.packageName);
            allInstalledPackages.add(packageInfo.packageName);
        }

        for (String packageName : fennecPackages) {
            if (!allInstalledPackages.contains(packageName)) {
                uninstalledPackages.add(packageName);
            }
        }

        if (uninstalledPackages.size() > 0) {
            JSONObject message = new JSONObject();
            JSONArray packageNames = new JSONArray();
            try {
                for (String packageName : uninstalledPackages) {
                    packageNames.put(packageName);
                }
                message.put("packages", packageNames);
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Webapps:AutoUninstall", message.toString()));
            } catch (JSONException e) {
                Log.e(LOGTAG, "JSON EXCEPTION " + e);
            }
        }
    }
}
