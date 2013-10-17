/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.ThreadUtils;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Collections;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.Scanner;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

public final class Distribution {
    private static final String LOGTAG = "GeckoDistribution";

    private static final String DEFAULT_PREFS = GeckoApp.PREFS_NAME;

    private static final int STATE_UNKNOWN = 0;
    private static final int STATE_NONE = 1;
    private static final int STATE_SET = 2;

    public static class DistributionDescriptor {
        public final boolean valid;
        public final String id;
        public final String version;    // Example uses a float, but that's a crazy idea.

        // Default UI-visible description of the distribution.
        public final String about;

        // Each distribution file can include multiple localized versions of
        // the 'about' string. These are represented as, e.g., "about.en-US"
        // keys in the Global object.
        // Here we map locale to description.
        public final Map<String, String> localizedAbout;

        @SuppressWarnings("unchecked")
        public DistributionDescriptor(JSONObject obj) {
            this.id = obj.optString("id");
            this.version = obj.optString("version");
            this.about = obj.optString("about");
            Map<String, String> loc = new HashMap<String, String>();
            try {
                Iterator<String> keys = obj.keys();
                while (keys.hasNext()) {
                    String key = keys.next();
                    if (key.startsWith("about.")) {
                        String locale = key.substring(6);
                        if (!obj.isNull(locale)) {
                            loc.put(locale, obj.getString(key));
                        }
                    }
                }
            } catch (JSONException ex) {
                Log.w(LOGTAG, "Unable to completely process distribution JSON.", ex);
            }

            this.localizedAbout = Collections.unmodifiableMap(loc);
            this.valid = (null != this.id) &&
                         (null != this.version) &&
                         (null != this.about);
        }
    }

    /**
     * Initializes distribution if it hasn't already been initalized. Sends
     * messages to Gecko as appropriate.
     *
     * @param packagePath where to look for the distribution directory.
     */
    public static void init(final Context context, final String packagePath, final String prefsPath) {
        // Read/write preferences and files on the background thread.
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                Distribution dist = new Distribution(context, packagePath, prefsPath);
                boolean distributionSet = dist.doInit();
                if (distributionSet) {
                    GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Distribution:Set", ""));
                }
            }
        });
    }

    /**
     * Use <code>Context.getPackageResourcePath</code> to find an implicit
     * package path.
     */
    public static void init(final Context context) {
        Distribution.init(context, context.getPackageResourcePath(), DEFAULT_PREFS);
    }

    /**
     * Returns parsed contents of bookmarks.json.
     * This method should only be called from a background thread.
     */
    public static JSONArray getBookmarks(final Context context) {
        Distribution dist = new Distribution(context);
        return dist.getBookmarks();
    }

    private final Context context;
    private final String packagePath;
    private final String prefsBranch;

    private int state = STATE_UNKNOWN;
    private File distributionDir = null;

    /**
     * @param packagePath where to look for the distribution directory.
     */
    public Distribution(final Context context, final String packagePath, final String prefsBranch) {
        this.context = context;
        this.packagePath = packagePath;
        this.prefsBranch = prefsBranch;
    }

    public Distribution(final Context context) {
        this(context, context.getPackageResourcePath(), DEFAULT_PREFS);
    }

    /**
     * Don't call from the main thread.
     *
     * @return true if we've set a distribution.
     */
    private boolean doInit() {
        // Bail if we've already tried to initialize the distribution, and
        // there wasn't one.
        SharedPreferences settings = context.getSharedPreferences(prefsBranch, Activity.MODE_PRIVATE);
        String keyName = context.getPackageName() + ".distribution_state";
        this.state = settings.getInt(keyName, STATE_UNKNOWN);
        if (this.state == STATE_NONE) {
            return false;
        }

        // We've done the work once; don't do it again.
        if (this.state == STATE_SET) {
            // Note that we don't compute the distribution directory.
            // Call `ensureDistributionDir` if you need it.
            return true;
        }

        boolean distributionSet = false;
        try {
            // First, try copying distribution files out of the APK.
            distributionSet = copyFiles();
            if (distributionSet) {
                // We always copy to the data dir, and we only copy files from
                // a 'distribution' subdirectory. Track our dist dir now that
                // we know it.
                this.distributionDir = new File(getDataDir(), "distribution/");
            }
        } catch (IOException e) {
            Log.e(LOGTAG, "Error copying distribution files", e);
        }

        if (!distributionSet) {
            // If there aren't any distribution files in the APK, look in the /system directory.
            File distDir = getSystemDistributionDir();
            if (distDir.exists()) {
                distributionSet = true;
                this.distributionDir = distDir;
            }
        }

        this.state = distributionSet ? STATE_SET : STATE_NONE;
        settings.edit().putInt(keyName, this.state).commit();
        return distributionSet;
    }

    /**
     * Copies the /distribution folder out of the APK and into the app's data directory.
     * Returns true if distribution files were found and copied.
     */
    private boolean copyFiles() throws IOException {
        File applicationPackage = new File(packagePath);
        ZipFile zip = new ZipFile(applicationPackage);

        boolean distributionSet = false;
        Enumeration<? extends ZipEntry> zipEntries = zip.entries();

        byte[] buffer = new byte[1024];
        while (zipEntries.hasMoreElements()) {
            ZipEntry fileEntry = zipEntries.nextElement();
            String name = fileEntry.getName();

            if (!name.startsWith("distribution/")) {
                continue;
            }

            distributionSet = true;

            File outFile = new File(getDataDir(), name);
            File dir = outFile.getParentFile();

            if (!dir.exists()) {
                if (!dir.mkdirs()) {
                    Log.e(LOGTAG, "Unable to create directories: " + dir.getAbsolutePath());
                    continue;
                }
            }

            InputStream fileStream = zip.getInputStream(fileEntry);
            OutputStream outStream = new FileOutputStream(outFile);

            int count;
            while ((count = fileStream.read(buffer)) != -1) {
                outStream.write(buffer, 0, count);
            }

            fileStream.close();
            outStream.close();
            outFile.setLastModified(fileEntry.getTime());
        }

        zip.close();

        return distributionSet;
    }

    /**
     * After calling this method, either <code>distributionDir</code>
     * will be set, or there is no distribution in use.
     *
     * Only call after init.
     */
    private File ensureDistributionDir() {
        if (this.distributionDir != null) {
            return this.distributionDir;
        }

        if (this.state != STATE_SET) {
            return null;
        }

        // After init, we know that either we've copied a distribution out of
        // the APK, or it exists in /system/.
        // Look in each location in turn.
        // (This could be optimized by caching the path in shared prefs.)
        File copied = new File(getDataDir(), "distribution/");
        if (copied.exists()) {
            return this.distributionDir = copied;
        }
        File system = getSystemDistributionDir();
        if (system.exists()) {
            return this.distributionDir = system;
        }
        return null;
    }

    /**
     * Helper to grab a file in the distribution directory.
     *
     * Returns null if there is no distribution directory or the file
     * doesn't exist. Ensures init first.
     */
    private File getDistributionFile(String name) {
        Log.i(LOGTAG, "Getting file from distribution.");
        if (this.state == STATE_UNKNOWN) {
            if (!this.doInit()) {
                return null;
            }
        }

        File dist = ensureDistributionDir();
        if (dist == null) {
            return null;
        }

        File descFile = new File(dist, name);
        if (!descFile.exists()) {
            Log.e(LOGTAG, "Distribution directory exists, but no file named " + name);
            return null;
        }

        return descFile;
    }

    public DistributionDescriptor getDescriptor() {
        File descFile = getDistributionFile("preferences.json");
        if (descFile == null) {
            // Logging and existence checks are handled in getDistributionFile.
            return null;
        }

        try {
            JSONObject all = new JSONObject(getFileContents(descFile));

            if (!all.has("Global")) {
                Log.e(LOGTAG, "Distribution preferences.json has no Global entry!");
                return null;
            }

            return new DistributionDescriptor(all.getJSONObject("Global"));

        } catch (IOException e) {
            Log.e(LOGTAG, "Error getting distribution descriptor file.", e);
            return null;
        } catch (JSONException e) {
            Log.e(LOGTAG, "Error parsing preferences.json", e);
            return null;
        }
    }

    public JSONArray getBookmarks() {
        File bookmarks = getDistributionFile("bookmarks.json");
        if (bookmarks == null) {
            // Logging and existence checks are handled in getDistributionFile.
            return null;
        }

        try {
            return new JSONArray(getFileContents(bookmarks));
        } catch (IOException e) {
            Log.e(LOGTAG, "Error getting bookmarks", e);
        } catch (JSONException e) {
            Log.e(LOGTAG, "Error parsing bookmarks.json", e);
        }

        return null;
    }

    // Shortcut to slurp a file without messing around with streams.
    private String getFileContents(File file) throws IOException {
        Scanner scanner = null;
        try {
            scanner = new Scanner(file, "UTF-8");
            return scanner.useDelimiter("\\A").next();
        } finally {
            if (scanner != null) {
                scanner.close();
            }
        }
    }

    private String getDataDir() {
        return context.getApplicationInfo().dataDir;
    }

    private File getSystemDistributionDir() {
        return new File("/system/" + context.getPackageName() + "/distribution");
    }
}
