/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.webapp;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.util.ThreadUtils;

import android.app.ActivityManager;
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
import java.util.Locale;
import java.util.Set;
import java.util.ArrayList;

public class UninstallListener extends BroadcastReceiver {

    private static final String LOGTAG = "GeckoWebappUninstallListener";

    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent.getBooleanExtra(Intent.EXTRA_REPLACING, false)) {
            Log.i(LOGTAG, "Package is being replaced; ignoring removal intent");
            return;
        }

        String packageName = intent.getData().getSchemeSpecificPart();

        if (TextUtils.isEmpty(packageName)) {
            Log.i(LOGTAG, "No package name defined in intent");
            return;
        }

        Allocator allocator = Allocator.getInstance(context);
        ArrayList<String> installedPackages = allocator.getInstalledPackageNames();

        if (installedPackages.contains(packageName)) {
            doUninstall(context, packageName);
        }
    }

    private static void doUninstall(Context context, String packageName) {
        ArrayList<String> uninstalledPackages = new ArrayList<String>();
        uninstalledPackages.add(packageName);
        doUninstall(context, uninstalledPackages);
    }

    private static void doUninstall(Context context, ArrayList<String> packageNames) {
        Allocator allocator = Allocator.getInstance(context);
        JSONObject message = new JSONObject();
        JSONArray jsonPackages = new JSONArray();

        for (String packageName : packageNames) {
            // Although its unlikely that an app is not allocated, but is installed in Gecko, it
            // is possible. We always send the packageName to JS to be removed from Gecko's registry.
            jsonPackages.put(packageName);

            int index = allocator.getIndexForApp(packageName);

            // If -1, nothing more to do; we didn't think it was installed anyway.
            if (index == -1)
                continue;

            allocator.releaseIndex(index);

            // kill the app if it's running
            String targetProcessName = context.getPackageName();
            targetProcessName = targetProcessName + ":" + targetProcessName + ".Webapp" + index;

            ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
            List<ActivityManager.RunningAppProcessInfo> procs = am.getRunningAppProcesses();
            if (procs != null) {
                for (ActivityManager.RunningAppProcessInfo proc : procs) {
                    if (proc.processName.equals(targetProcessName)) {
                        android.os.Process.killProcess(proc.pid);
                        break;
                    }
                }
            }

            // then nuke the profile
            GeckoProfile.removeProfile(context, "webapp" + index);
        }

        try {
            message.put("apkPackageNames", jsonPackages);
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Webapps:AutoUninstall", message.toString()));
        } catch (JSONException e) {
            Log.e(LOGTAG, "Error sending uninstall packages to Gecko", e);
        }
    }

    public static void initUninstallPackageScan(Context context) {
        // get list of packages we think are installed
        Allocator allocator = Allocator.getInstance(context);
        ArrayList<String> fennecPackages = allocator.getInstalledPackageNames();
        ArrayList<String> uninstalledPackages = new ArrayList<String>();

        final PackageManager pm = context.getPackageManager();
        //get a list of installed apps on device
        List<ApplicationInfo> packages = pm.getInstalledApplications(PackageManager.GET_META_DATA);
        Set<String> allInstalledPackages = new HashSet<String>();

        for (ApplicationInfo packageInfo : packages) {
            allInstalledPackages.add(packageInfo.packageName);
        }

        for (String packageName : fennecPackages) {
            if (!allInstalledPackages.contains(packageName)) {
                uninstalledPackages.add(packageName);
            }
        }

        if (uninstalledPackages.size() > 0) {
            doUninstall(context, uninstalledPackages);
        }
    }

    public static class DelayedStartupTask implements Runnable {
        private final GeckoApp mApp;

        public DelayedStartupTask(GeckoApp app) {
            mApp = app;
        }

        @Override
        public void run() {
            ThreadUtils.assertOnBackgroundThread();

            // Perform webapp uninstalls as appropiate.
            UninstallListener.initUninstallPackageScan(mApp.getApplicationContext());
        }
    }
}
