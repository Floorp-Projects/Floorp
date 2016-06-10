/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.updater;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

import com.keepsafe.switchboard.SwitchBoard;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.delegates.BrowserAppDelegateWithReference;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.util.ThreadUtils;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.Enumeration;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

/**
 * Perform tasks in the background after the app has been installed/updated.
 */
public class PostUpdateHandler extends BrowserAppDelegateWithReference {
    private static final String LOGTAG = "PostUpdateHandler";

    @Override
    public void onStart(BrowserApp browserApp) {
        final SharedPreferences prefs = GeckoSharedPrefs.forApp(browserApp);

        // Check if this is a new installation or if the app has been updated since the last start.
        if (!AppConstants.MOZ_APP_BUILDID.equals(prefs.getString(GeckoPreferences.PREFS_APP_UPDATE_LAST_BUILD_ID, null))) {
            Log.d(LOGTAG, "Build ID changed since last start: '" + AppConstants.MOZ_APP_BUILDID + "', '" + prefs.getString(GeckoPreferences.PREFS_APP_UPDATE_LAST_BUILD_ID, null) + "'");

            // Copy the bundled system add-ons from the APK to the data directory.
            copyFeaturesFromAPK();
        }
    }

    /**
     * Copies the /assets/features folder out of the APK and into the app's data directory.
     */
    private void copyFeaturesFromAPK() {
        final BrowserApp browserApp = getBrowserApp();
        if (browserApp == null) {
            return;
        }

        final String dataDir = browserApp.getApplicationInfo().dataDir;
        final String sourceDir = browserApp.getApplicationInfo().sourceDir;
        final File applicationPackage = new File(sourceDir);

        final String assetsPrefix = "assets/";
        final String fullPrefix = assetsPrefix + "features/";

        final SharedPreferences prefs = GeckoSharedPrefs.forApp(browserApp);

        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                Log.d(LOGTAG, "Copying system add-ons from APK to dataDir");

                try {
                    final ZipFile zip = new ZipFile(applicationPackage);
                    final Enumeration<? extends ZipEntry> zipEntries = zip.entries();
                    
                    final byte[] buffer = new byte[1024];

                    while (zipEntries.hasMoreElements()) {
                        final ZipEntry fileEntry = zipEntries.nextElement();
                        final String name = fileEntry.getName();

                        if (fileEntry.isDirectory()) {
                            // We'll let getDataFile deal with creating the directory hierarchy.
                            continue;
                        }

                        // Read from "assets/features/**".
                        if (!name.startsWith(fullPrefix)) {
                            continue;
                        }

                        // Write to "features/**".
                        final String nameWithoutPrefix = name.substring(assetsPrefix.length());
                        final File outFile = getDataFile(dataDir, nameWithoutPrefix);
                        if (outFile == null) {
                            continue;
                        }

                        final InputStream fileStream = zip.getInputStream(fileEntry);
                        try {
                            writeStream(fileStream, outFile, fileEntry.getTime(), buffer);
                        } finally {
                            fileStream.close();
                        }
                    }

                    zip.close();
                } catch (IOException e) {
                    Log.e(LOGTAG, "Error copying system add-ons from APK.", e);
                }

                // Save the Build ID so we don't perform post-update operations again until the app is updated.
                prefs.edit().putString(GeckoPreferences.PREFS_APP_UPDATE_LAST_BUILD_ID, AppConstants.MOZ_APP_BUILDID).apply();
            }
        });
    }

    /**
     * Return a File instance in the data directory, ensuring
     * that the parent exists.
     *
     * @return null if the parents could not be created.
     */
    private File getDataFile(final String dataDir, final String name) {
        File outFile = new File(dataDir, name);
        File dir = outFile.getParentFile();

        if (!dir.exists()) {
            Log.d(LOGTAG, "Creating " + dir.getAbsolutePath());
            if (!dir.mkdirs()) {
                Log.e(LOGTAG, "Unable to create directories: " + dir.getAbsolutePath());
                return null;
            }
        }

        return outFile;
    }

    private void writeStream(InputStream fileStream, File outFile, final long modifiedTime, byte[] buffer)
            throws FileNotFoundException, IOException {
        final OutputStream outStream = new FileOutputStream(outFile);
        try {
            int count;
            while ((count = fileStream.read(buffer)) > 0) {
                outStream.write(buffer, 0, count);
            }

            outFile.setLastModified(modifiedTime);
        } finally {
            outStream.close();
        }
    }
}
