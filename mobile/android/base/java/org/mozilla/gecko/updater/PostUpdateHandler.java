/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.updater;

import android.content.SharedPreferences;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.util.Log;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.util.IOUtils;
import org.mozilla.gecko.util.StrictModeContext;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Perform <b>synchronous</b> tasks after the app has been installed/updated.
 *
 * This is <b>only</b> intended for things that race profile creation and/or first profile read.
 * Use (or introduce) an asynchronous vehicle for things that don't race one of those two
 * operations!
 */
public class PostUpdateHandler {
    private static final String LOGTAG = "GeckoPostUpdateHandler";
    private static final boolean DEBUG = false;

    @SuppressWarnings("try")
    public void onCreate(final BrowserApp browserApp, final Bundle savedInstanceState) {
        // Copying features out the APK races Gecko startup: the first time the profile is read by
        // Gecko, it needs to find the copied features.  Rather than do non-trivial synchronization
        // to avoid the race, just do the work synchronously.
        try (StrictModeContext unused = StrictModeContext.allowDiskWrites()) {
            final SharedPreferences prefs = GeckoSharedPrefs.forApp(browserApp);

            // Check if this is a new installation or if the app has been updated since the last start.
            if (!AppConstants.MOZ_APP_BUILDID.equals(prefs.getString(GeckoPreferences.PREFS_APP_UPDATE_LAST_BUILD_ID, null))) {
                if (DEBUG) {
                    Log.d(LOGTAG, "Build ID changed since last start: '" +
                            AppConstants.MOZ_APP_BUILDID +
                            "', '" +
                            prefs.getString(GeckoPreferences.PREFS_APP_UPDATE_LAST_BUILD_ID, null)
                            + "'");
                }

                // Copy the bundled system add-ons from the APK to the data directory.
                copyFeaturesFromAPK(browserApp);
            }
        }
    }

    /**
     * Copies the /assets/features folder out of the APK and into the app's data directory.
     */
    private void copyFeaturesFromAPK(BrowserApp browserApp) {
        if (DEBUG) {
            Log.d(LOGTAG, "Copying system add-ons from APK to dataDir");
        }

        final String dataDir = browserApp.getApplicationInfo().dataDir;
        final SharedPreferences prefs = GeckoSharedPrefs.forApp(browserApp);
        final AssetManager assetManager = browserApp.getAssets();

        try {
            final String[] assetNames = assetManager.list("features");

            for (int i = 0; i < assetNames.length; i++) {
                final String assetPath = "features/" + assetNames[i];

                if (DEBUG) {
                    Log.d(LOGTAG, "Copying '" + assetPath + "' from APK to dataDir");
                }

                final InputStream assetStream = assetManager.open(assetPath);
                final File outFile = getDataFile(dataDir, assetPath);

                if (outFile == null) {
                    continue;
                }

                final OutputStream outStream = new FileOutputStream(outFile);

                try {
                    IOUtils.copy(assetStream, outStream);
                } catch (IOException e) {
                    Log.e(LOGTAG, "Error copying '" + assetPath + "' from APK to dataDir");
                } finally {
                    outStream.close();
                }
            }
        } catch (IOException e) {
            Log.e(LOGTAG, "Error retrieving packaged system add-ons from APK", e);
        }

        // Save the Build ID so we don't perform post-update operations again until the app is updated.
        prefs.edit().putString(GeckoPreferences.PREFS_APP_UPDATE_LAST_BUILD_ID, AppConstants.MOZ_APP_BUILDID).apply();
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
            if (DEBUG) {
                Log.d(LOGTAG, "Creating " + dir.getAbsolutePath());
            }
            if (!dir.mkdirs()) {
                Log.e(LOGTAG, "Unable to create directories: " + dir.getAbsolutePath());
                return null;
            }
        }

        return outFile;
    }
}
