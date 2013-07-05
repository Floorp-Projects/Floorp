/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko;

import org.mozilla.gecko.util.ThreadUtils;

import org.json.JSONArray;
import org.json.JSONException;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.util.Enumeration;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

public final class Distribution {
    private static final String LOGTAG = "GeckoDistribution";

    private static final int STATE_UNKNOWN = 0;
    private static final int STATE_NONE = 1;
    private static final int STATE_SET = 2;

    /**
     * Initializes distribution if it hasn't already been initalized.
     *
     * @param packagePath specifies where to look for the distribution directory.
     */
    public static void init(final Context context, final String packagePath) {
        // Read/write preferences and files on the background thread.
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                // Bail if we've already initialized the distribution.
                SharedPreferences settings = context.getSharedPreferences(GeckoApp.PREFS_NAME, Activity.MODE_PRIVATE);
                String keyName = context.getPackageName() + ".distribution_state";
                int state = settings.getInt(keyName, STATE_UNKNOWN);
                if (state == STATE_NONE) {
                    return;
                }

                // Send a message to Gecko if we've set a distribution.
                if (state == STATE_SET) {
                    GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Distribution:Set", ""));
                    return;
                }

                boolean distributionSet = false;
                try {
                    // First, try copying distribution files out of the APK.
                    distributionSet = copyFiles(context, packagePath);
                } catch (IOException e) {
                    Log.e(LOGTAG, "Error copying distribution files", e);
                }

                if (!distributionSet) {
                    // If there aren't any distribution files in the APK, look in the /system directory.
                    File distDir = new File("/system/" + context.getPackageName() + "/distribution");
                    if (distDir.exists()) {
                        distributionSet = true;
                    }
                }

                if (distributionSet) {
                    GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Distribution:Set", ""));
                    settings.edit().putInt(keyName, STATE_SET).commit();
                } else {
                    settings.edit().putInt(keyName, STATE_NONE).commit();
                }
            }
        });
    }

    /**
     * Copies the /distribution folder out of the APK and into the app's data directory.
     * Returns true if distribution files were found and copied.
     */
    private static boolean copyFiles(Context context, String packagePath) throws IOException {
        File applicationPackage = new File(packagePath);
        ZipFile zip = new ZipFile(applicationPackage);

        boolean distributionSet = false;
        Enumeration<? extends ZipEntry> zipEntries = zip.entries();
        while (zipEntries.hasMoreElements()) {
            ZipEntry fileEntry = zipEntries.nextElement();
            String name = fileEntry.getName();

            if (!name.startsWith("distribution/"))
                continue;

            distributionSet = true;

            File dataDir = new File(context.getApplicationInfo().dataDir);
            File outFile = new File(dataDir, name);

            File dir = outFile.getParentFile();
            if (!dir.exists())
                dir.mkdirs();

            InputStream fileStream = zip.getInputStream(fileEntry);
            OutputStream outStream = new FileOutputStream(outFile);

            int b;
            while ((b = fileStream.read()) != -1)
                outStream.write(b);

            fileStream.close();
            outStream.close();
            outFile.setLastModified(fileEntry.getTime());
        }

        zip.close();

        return distributionSet;
    }

    /**
     * Returns parsed contents of bookmarks.json.
     * This method should only be called from a background thread.
     */
    public static JSONArray getBookmarks(Context context) {
        SharedPreferences settings = context.getSharedPreferences(GeckoApp.PREFS_NAME, Activity.MODE_PRIVATE);
        String keyName = context.getPackageName() + ".distribution_state";
        int state = settings.getInt(keyName, STATE_UNKNOWN);
        if (state == STATE_NONE) {
            return null;
        }

        ZipFile zip = null;
        InputStream inputStream = null;
        try {
            if (state == STATE_UNKNOWN) {
                // If the distribution hasn't been set yet, first look for bookmarks.json in the APK.
                File applicationPackage = new File(context.getPackageResourcePath());
                zip = new ZipFile(applicationPackage);
                ZipEntry zipEntry = zip.getEntry("distribution/bookmarks.json");
                if (zipEntry != null) {
                    inputStream = zip.getInputStream(zipEntry);
                } else {
                    // If there's no bookmarks.json in the APK, but there is a preferences.json,
                    // don't create any distribution bookmarks.
                    zipEntry = zip.getEntry("distribution/preferences.json");
                    if (zipEntry != null) {
                        return null;
                    }
                    // Otherwise, look for bookmarks.json in the /system directory.
                    File systemFile = new File("/system/" + context.getPackageName() + "/distribution/bookmarks.json");
                    if (!systemFile.exists()) {
                        return null;
                    }
                    inputStream = new FileInputStream(systemFile);
                }
            } else {
                // Otherwise, first look for the distribution in the data directory.
                File distDir = new File(context.getApplicationInfo().dataDir, "distribution");
                if (!distDir.exists()) {
                    // If that doesn't exist, then we must be using a distribution from the system directory.
                    distDir = new File("/system/" + context.getPackageName() + "/distribution");
                }

                File file = new File(distDir, "bookmarks.json");
                inputStream = new FileInputStream(file);
            }

            // Convert input stream to JSONArray
            BufferedReader reader = new BufferedReader(new InputStreamReader(inputStream));
            StringBuilder stringBuilder = new StringBuilder();
            String s;
            while ((s = reader.readLine()) != null) {
                stringBuilder.append(s);
            }
            return new JSONArray(stringBuilder.toString());
        } catch (IOException e) {
            Log.e(LOGTAG, "Error getting bookmarks", e);
        } catch (JSONException e) {
            Log.e(LOGTAG, "Error parsing bookmarks.json", e);
        } finally {
            try {
                if (zip != null) {
                    zip.close();
                }
                if (inputStream != null) {
                    inputStream.close();
                }
            } catch (IOException e) {
                Log.e(LOGTAG, "Error closing streams", e);
            } 
        }
        return null;
    }
}
