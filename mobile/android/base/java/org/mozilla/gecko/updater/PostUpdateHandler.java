/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.updater;

import android.content.Context;
import android.content.res.AssetManager;
import android.content.SharedPreferences;
import android.util.Log;

import com.keepsafe.switchboard.SwitchBoard;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.delegates.BrowserAppDelegateWithReference;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.util.IOUtils;
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
    public void onStart(final BrowserApp browserApp) {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                final SharedPreferences prefs = GeckoSharedPrefs.forApp(browserApp);

                // Check if this is a new installation or if the app has been updated since the last start.
                if (!AppConstants.MOZ_APP_BUILDID.equals(prefs.getString(GeckoPreferences.PREFS_APP_UPDATE_LAST_BUILD_ID, null))) {
                    Log.d(LOGTAG, "Build ID changed since last start: '" + AppConstants.MOZ_APP_BUILDID + "', '" + prefs.getString(GeckoPreferences.PREFS_APP_UPDATE_LAST_BUILD_ID, null) + "'");

                    // Copy the bundled system add-ons from the APK to the data directory.
                    copyFeaturesFromAPK(browserApp);
                }
            }
        });
    }

    /**
     * Copies the /assets/features folder out of the APK and into the app's data directory.
     */
    private void copyFeaturesFromAPK(BrowserApp browserApp) {
        Log.d(LOGTAG, "Copying system add-ons from APK to dataDir");

        final String dataDir = browserApp.getApplicationInfo().dataDir;
        final SharedPreferences prefs = GeckoSharedPrefs.forApp(browserApp);
        final AssetManager assetManager = browserApp.getContext().getAssets();

        try {
            final String[] assetNames = assetManager.list("features");

            for (int i = 0; i < assetNames.length; i++) {
                final String assetPath = "features/" + assetNames[i];

                Log.d(LOGTAG, "Copying '" + assetPath + "' from APK to dataDir");
                
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
            Log.d(LOGTAG, "Creating " + dir.getAbsolutePath());
            if (!dir.mkdirs()) {
                Log.e(LOGTAG, "Unable to create directories: " + dir.getAbsolutePath());
                return null;
            }
        }

        return outFile;
    }
}
