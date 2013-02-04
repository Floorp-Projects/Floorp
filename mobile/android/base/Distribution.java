/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko;

import org.mozilla.gecko.util.GeckoBackgroundThread;

import android.app.Activity;
import android.content.SharedPreferences;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
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
     */
    public static void init(final Activity activity) {
        // Read/write preferences and files on the background thread.
        GeckoBackgroundThread.getHandler().post(new Runnable() {
            public void run() {
                // Bail if we've already initialized the distribution.
                SharedPreferences settings = activity.getPreferences(Activity.MODE_PRIVATE);
                String keyName = activity.getPackageName() + ".distribution_state";
                int state = settings.getInt(keyName, STATE_UNKNOWN);
                if (state == STATE_NONE)
                    return;

                // Send a message to Gecko if we've set a distribution.
                if (state == STATE_SET) {
                    GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Distribution:Set", null));
                    return;
                }

                boolean distributionSet = false;
                try {
                    distributionSet = copyFiles(activity);
                } catch (IOException e) {
                    Log.e(LOGTAG, "Error copying distribution files", e);
                }

                if (distributionSet) {
                    GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Distribution:Set", null));
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
    private static boolean copyFiles(Activity activity) throws IOException {
        File applicationPackage = new File(activity.getPackageResourcePath());
        ZipFile zip = new ZipFile(applicationPackage);

        boolean distributionSet = false;
        Enumeration<? extends ZipEntry> zipEntries = zip.entries();
        while (zipEntries.hasMoreElements()) {
            ZipEntry fileEntry = zipEntries.nextElement();
            String name = fileEntry.getName();

            if (!name.startsWith("distribution/"))
                continue;

            distributionSet = true;

            File dataDir = new File(activity.getApplicationInfo().dataDir);
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
}
