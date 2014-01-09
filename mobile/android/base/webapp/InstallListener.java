/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.webapp;

import java.io.File;
import java.io.IOException;
import java.net.MalformedURLException;
import java.net.URL;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.GeckoThread;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Environment;
import android.text.TextUtils;
import android.util.Log;

public class InstallListener extends BroadcastReceiver {

    private static String LOGTAG = "GeckoInstallListener";
    private JSONObject mData = null;
    private String mManifestUrl;

    public InstallListener(String manifestUrl, JSONObject data) {
        mData = data;
        mManifestUrl = manifestUrl;
        assert mManifestUrl != null;
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        String packageName = intent.getData().getSchemeSpecificPart();

        if (TextUtils.isEmpty(packageName)) {
            Log.i(LOGTAG, "No package name defined in intent");
            return;
        }

        ApkResources apkResources = null;
        try {
            apkResources = new ApkResources(context, packageName);
        } catch (NameNotFoundException e) {
            Log.e(LOGTAG, "Can't find package that's just been installed");
            return;
        }

        String manifestUrl = apkResources.getManifestUrl();
        if (TextUtils.isEmpty(manifestUrl)) {
            Log.i(LOGTAG, "No manifest URL present in metadata");
            return;
        } else if (!isCorrectManifest(manifestUrl)) {
            Log.i(LOGTAG, "Waiting to finish installing " + mManifestUrl + " but this is " +manifestUrl);
            //return;
        }

        if (GeckoThread.checkLaunchState(GeckoThread.LaunchState.GeckoRunning)) {
            InstallHelper installHelper = new InstallHelper(context, apkResources, null);
            try {
                JSONObject dataObject = mData;
                dataObject = new JSONObject().put("request", dataObject);
                WebAppAllocator slots = WebAppAllocator.getInstance(context);
                int i = slots.findOrAllocatePackage(packageName);
                installHelper.startInstall("webapp" + i, dataObject);
            } catch (JSONException e) {
                Log.e(LOGTAG, "Couldn't parse data from mozApps.install()", e);
            } catch (IOException e) {
                Log.e(LOGTAG, "Couldn't install packaged app", e);
            }
        }

        cleanup();

        // we don't need this anymore.
        context.unregisterReceiver(this);
    }

    public boolean isCorrectManifest(String manifestUrl) {
        try {
            URL registered = new URL(mManifestUrl);
            URL observed = new URL(manifestUrl);
            // TODO: this should be matching the scheme, origin and path, but ignoring the query.
            // That doesn't seem to be happening.
            return registered.sameFile(observed);
        } catch (MalformedURLException e) {
            return false;
        }
    }

    public void cleanup() {
        String manifestUrlFilename = mManifestUrl.replaceAll("[^a-zA-Z0-9]", "");

        File apkFile = new File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS), manifestUrlFilename + ".apk");
        if (apkFile.exists()) {
            apkFile.delete();
            Log.i(LOGTAG, "Downloaded APK file deleted");
        }
    }
}
