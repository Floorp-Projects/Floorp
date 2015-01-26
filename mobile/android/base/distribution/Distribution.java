/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.distribution;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.ProtocolException;
import java.net.SocketException;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.UnknownHostException;
import java.util.Collections;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.Queue;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.jar.JarEntry;
import java.util.jar.JarInputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

import javax.net.ssl.SSLException;

import org.apache.http.protocol.HTTP;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.mozglue.RobocopTarget;
import org.mozilla.gecko.util.FileUtils;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.ThreadUtils;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.SystemClock;
import android.util.Log;

/**
 * Handles distribution file loading and fetching,
 * and the corresponding hand-offs to Gecko.
 */
@RobocopTarget
public class Distribution {
    private static final String LOGTAG = "GeckoDistribution";

    private static final int STATE_UNKNOWN = 0;
    private static final int STATE_NONE = 1;
    private static final int STATE_SET = 2;

    private static final String FETCH_PROTOCOL = "https";
    private static final String FETCH_HOSTNAME = "mobile.cdn.mozilla.net";
    private static final String FETCH_PATH = "/distributions/1/";
    private static final String FETCH_EXTENSION = ".jar";

    private static final String EXPECTED_CONTENT_TYPE = "application/java-archive";

    private static final String DISTRIBUTION_PATH = "distribution/";

    /**
     * Telemetry constants.
     */
    private static final String HISTOGRAM_REFERRER_INVALID = "FENNEC_DISTRIBUTION_REFERRER_INVALID";
    private static final String HISTOGRAM_DOWNLOAD_TIME_MS = "FENNEC_DISTRIBUTION_DOWNLOAD_TIME_MS";
    private static final String HISTOGRAM_CODE_CATEGORY = "FENNEC_DISTRIBUTION_CODE_CATEGORY";

    /**
     * Success/failure codes. Don't exceed the maximum listed in Histograms.json.
     */
    private static final int CODE_CATEGORY_STATUS_OUT_OF_RANGE = 0;
    // HTTP status 'codes' run from 1 to 5.
    private static final int CODE_CATEGORY_OFFLINE = 6;
    private static final int CODE_CATEGORY_FETCH_EXCEPTION = 7;

    // It's a post-fetch exception if we were able to download, but not
    // able to extract.
    private static final int CODE_CATEGORY_POST_FETCH_EXCEPTION = 8;
    private static final int CODE_CATEGORY_POST_FETCH_SECURITY_EXCEPTION = 9;

    // It's a malformed distribution if we could extract, but couldn't
    // process the contents.
    private static final int CODE_CATEGORY_MALFORMED_DISTRIBUTION = 10;

    // Specific fetch errors.
    private static final int CODE_CATEGORY_FETCH_SOCKET_ERROR = 11;
    private static final int CODE_CATEGORY_FETCH_SSL_ERROR = 12;
    private static final int CODE_CATEGORY_FETCH_NON_SUCCESS_RESPONSE = 13;
    private static final int CODE_CATEGORY_FETCH_INVALID_CONTENT_TYPE = 14;

    // Corresponds to the high value in Histograms.json.
    private static final long MAX_DOWNLOAD_TIME_MSEC = 40000;    // 40 seconds.

    // If this is true, ready callbacks that arrive after our state is initially determined
    // will be queued for delayed running.
    // This should only be the case on first run, when we're in STATE_NONE.
    // Implicitly accessed from any non-UI threads via Distribution.doInit, but in practice only one
    // will actually perform initialization, and "non-UI thread" really means "background thread".
    private volatile boolean shouldDelayLateCallbacks = false;

    /**
     * These tasks can be queued to run when a distribution is available.
     *
     * If <code>distributionFound</code> is called, it will be the only call.
     * If <code>distributionNotFound</code> is called, it might be followed by
     * a call to <code>distributionArrivedLate</code>.
     *
     * When <code>distributionNotFound</code> is called,
     * {@link org.mozilla.gecko.distribution.Distribution#exists()} will return
     * false. In the other two callbacks, it will return true.
     */
    public interface ReadyCallback {
        void distributionNotFound();
        void distributionFound(Distribution distribution);
        void distributionArrivedLate(Distribution distribution);
    }

    /**
     * Used as a drop-off point for ReferrerReceiver. Checked when we process
     * first-run distribution.
     *
     * This is `protected` so that test code can clear it between runs.
     */
    @RobocopTarget
    protected static volatile ReferrerDescriptor referrer;

    private static Distribution instance;

    private final Context context;
    private final String packagePath;
    private final String prefsBranch;

    volatile int state = STATE_UNKNOWN;
    private File distributionDir;

    private final Queue<ReadyCallback> onDistributionReady = new ConcurrentLinkedQueue<>();

    // Callbacks in this queue have been invoked once as distributionNotFound.
    // If they're invoked again, it'll be with distributionArrivedLate.
    private final Queue<ReadyCallback> onLateReady = new ConcurrentLinkedQueue<>();

    /**
     * This is a little bit of a bad singleton, because in principle a Distribution
     * can be created with arbitrary paths. So we only have one path to get here, and
     * it uses the default arguments. Watch out if you're creating your own instances!
     */
    public static synchronized Distribution getInstance(Context context) {
        if (instance == null) {
            instance = new Distribution(context);
        }
        return instance;
    }

    @RobocopTarget
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

    private static Distribution init(final Distribution distribution) {
        // Read/write preferences and files on the background thread.
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                boolean distributionSet = distribution.doInit();
                if (distributionSet) {
                    GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Distribution:Set", ""));
                }
            }
        });

        return distribution;
    }

    /**
     * Initializes distribution if it hasn't already been initialized. Sends
     * messages to Gecko as appropriate.
     *
     * @param packagePath where to look for the distribution directory.
     */
    @RobocopTarget
    public static Distribution init(final Context context, final String packagePath, final String prefsPath) {
        return init(new Distribution(context, packagePath, prefsPath));
    }

    /**
     * Use <code>Context.getPackageResourcePath</code> to find an implicit
     * package path. Reuses the existing Distribution if one exists.
     */
    @RobocopTarget
    public static Distribution init(final Context context) {
        return init(Distribution.getInstance(context));
    }

    /**
     * Returns parsed contents of bookmarks.json.
     * This method should only be called from a background thread.
     */
    public static JSONArray getBookmarks(final Context context) {
        Distribution dist = new Distribution(context);
        return dist.getBookmarks();
    }

    /**
     * @param packagePath where to look for the distribution directory.
     */
    public Distribution(final Context context, final String packagePath, final String prefsBranch) {
        this.context = context;
        this.packagePath = packagePath;
        this.prefsBranch = prefsBranch;
    }

    public Distribution(final Context context) {
        this(context, context.getPackageResourcePath(), null);
    }

    /**
     * This method is called by ReferrerReceiver when we receive a post-install
     * notification from Google Play.
     *
     * @param ref a parsed referrer value from the store-supplied intent.
     */
    public static void onReceivedReferrer(final Context context, final ReferrerDescriptor ref) {
        // Track the referrer object for distribution handling.
        referrer = ref;

        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                final Distribution distribution = Distribution.getInstance(context);

                // This will bail if we aren't delayed, or we already have a distribution.
                distribution.processDelayedReferrer(ref);
            }
        });
    }

    /**
     * Handle a referrer intent that arrives after first use of the distribution.
     */
    private void processDelayedReferrer(final ReferrerDescriptor ref) {
        ThreadUtils.assertOnBackgroundThread();
        if (state != STATE_NONE) {
            return;
        }

        Log.i(LOGTAG, "Processing delayed referrer.");

        if (!checkIntentDistribution(ref)) {
            // Oh well. No sense keeping these tasks around.
            this.onLateReady.clear();
            return;
        }

        // Persist our new state.
        this.state = STATE_SET;
        getSharedPreferences().edit().putInt(getKeyName(), this.state).apply();

        // Just in case this isn't empty but doInit has finished.
        runReadyQueue();

        // Now process any tasks that already ran while we were in STATE_NONE
        // to tell them of our good news.
        runLateReadyQueue();

        // Make sure that changes to search defaults are applied immediately.
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Distribution:Changed", ""));
    }

    /**
     * Helper to grab a file in the distribution directory.
     *
     * Returns null if there is no distribution directory or the file
     * doesn't exist. Ensures init first.
     */
    public File getDistributionFile(String name) {
        Log.d(LOGTAG, "Getting file from distribution.");

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
            JSONObject all = new JSONObject(FileUtils.getFileContents(descFile));

            if (!all.has("Global")) {
                Log.e(LOGTAG, "Distribution preferences.json has no Global entry!");
                return null;
            }

            return new DistributionDescriptor(all.getJSONObject("Global"));

        } catch (IOException e) {
            Log.e(LOGTAG, "Error getting distribution descriptor file.", e);
            Telemetry.addToHistogram(HISTOGRAM_CODE_CATEGORY, CODE_CATEGORY_MALFORMED_DISTRIBUTION);
            return null;
        } catch (JSONException e) {
            Log.e(LOGTAG, "Error parsing preferences.json", e);
            Telemetry.addToHistogram(HISTOGRAM_CODE_CATEGORY, CODE_CATEGORY_MALFORMED_DISTRIBUTION);
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
            return new JSONArray(FileUtils.getFileContents(bookmarks));
        } catch (IOException e) {
            Log.e(LOGTAG, "Error getting bookmarks", e);
            Telemetry.addToHistogram(HISTOGRAM_CODE_CATEGORY, CODE_CATEGORY_MALFORMED_DISTRIBUTION);
            return null;
        } catch (JSONException e) {
            Log.e(LOGTAG, "Error parsing bookmarks.json", e);
            Telemetry.addToHistogram(HISTOGRAM_CODE_CATEGORY, CODE_CATEGORY_MALFORMED_DISTRIBUTION);
            return null;
        }
    }

    /**
     * Don't call from the main thread.
     *
     * Postcondition: if this returns true, distributionDir will have been
     * set and populated.
     *
     * This method is *only* protected for use from testDistribution.
     *
     * @return true if we've set a distribution.
     */
    @RobocopTarget
    protected boolean doInit() {
        ThreadUtils.assertNotOnUiThread();

        // Bail if we've already tried to initialize the distribution, and
        // there wasn't one.
        final SharedPreferences settings = getSharedPreferences();

        final String keyName = getKeyName();
        this.state = settings.getInt(keyName, STATE_UNKNOWN);

        if (this.state == STATE_NONE) {
            runReadyQueue();
            return false;
        }

        // We've done the work once; don't do it again.
        if (this.state == STATE_SET) {
            // Note that we don't compute the distribution directory.
            // Call `ensureDistributionDir` if you need it.
            runReadyQueue();
            return true;
        }

        // We try the install intent, then the APK, then the system directory.
        final boolean distributionSet =
                checkIntentDistribution(referrer) ||
                checkAPKDistribution() ||
                checkSystemDistribution();

        // If this is our first run -- and thus we weren't already in STATE_NONE or STATE_SET above --
        // and we didn't find a distribution already, then we should hold on to callbacks in case we
        // get a late distribution.
        this.shouldDelayLateCallbacks = !distributionSet;
        this.state = distributionSet ? STATE_SET : STATE_NONE;
        settings.edit().putInt(keyName, this.state).apply();

        runReadyQueue();
        return distributionSet;
    }

    /**
     * If applicable, download and select the distribution specified in
     * the referrer intent.
     *
     * @return true if a referrer-supplied distribution was selected.
     */
    private boolean checkIntentDistribution(final ReferrerDescriptor referrer) {
        if (referrer == null) {
            return false;
        }

        URI uri = getReferredDistribution(referrer);
        if (uri == null) {
            return false;
        }

        long start = SystemClock.uptimeMillis();
        Log.v(LOGTAG, "Downloading referred distribution: " + uri);

        try {
            final HttpURLConnection connection = (HttpURLConnection) uri.toURL().openConnection();

            // If the Search Activity starts, and we handle the referrer intent, this'll return
            // null. Recover gracefully in this case.
            final GeckoAppShell.GeckoInterface geckoInterface = GeckoAppShell.getGeckoInterface();
            final String ua;
            if (geckoInterface == null) {
                // Fall back to GeckoApp's default implementation.
                ua = HardwareUtils.isTablet() ? AppConstants.USER_AGENT_FENNEC_TABLET :
                                                AppConstants.USER_AGENT_FENNEC_MOBILE;
            } else {
                ua = geckoInterface.getDefaultUAString();
            }

            connection.setRequestProperty(HTTP.USER_AGENT, ua);
            connection.setRequestProperty("Accept", EXPECTED_CONTENT_TYPE);

            try {
                final JarInputStream distro;
                try {
                    distro = fetchDistribution(uri, connection);
                } catch (Exception e) {
                    Log.e(LOGTAG, "Error fetching distribution from network.", e);
                    recordFetchTelemetry(e);
                    return false;
                }

                long end = SystemClock.uptimeMillis();
                final long duration = end - start;
                Log.d(LOGTAG, "Distro fetch took " + duration + "ms; result? " + (distro != null));
                Telemetry.addToHistogram(HISTOGRAM_DOWNLOAD_TIME_MS, clamp(MAX_DOWNLOAD_TIME_MSEC, duration));

                if (distro == null) {
                    // Nothing to do.
                    return false;
                }

                // Try to copy distribution files from the fetched stream.
                try {
                    Log.d(LOGTAG, "Copying files from fetched zip.");
                    if (copyFilesFromStream(distro)) {
                        // We always copy to the data dir, and we only copy files from
                        // a 'distribution' subdirectory. Track our dist dir now that
                        // we know it.
                        this.distributionDir = new File(getDataDir(), DISTRIBUTION_PATH);
                        return true;
                    }
                } catch (SecurityException e) {
                    Log.e(LOGTAG, "Security exception copying files. Corrupt or malicious?", e);
                    Telemetry.addToHistogram(HISTOGRAM_CODE_CATEGORY, CODE_CATEGORY_POST_FETCH_SECURITY_EXCEPTION);
                } catch (Exception e) {
                    Log.e(LOGTAG, "Error copying files from distribution.", e);
                    Telemetry.addToHistogram(HISTOGRAM_CODE_CATEGORY, CODE_CATEGORY_POST_FETCH_EXCEPTION);
                } finally {
                    distro.close();
                }
            } finally {
                connection.disconnect();
            }
        } catch (IOException e) {
            Log.e(LOGTAG, "Error copying distribution files from network.", e);
            recordFetchTelemetry(e);
        }

        return false;
    }

    private static final int clamp(long v, long c) {
        return (int) Math.min(c, v);
    }

    /**
     * Fetch the provided URI, returning a {@link JarInputStream} if the response body
     * is appropriate.
     *
     * Protected to allow for mocking.
     *
     * @return the entity body as a stream, or null on failure.
     */
    @SuppressWarnings("static-method")
    @RobocopTarget
    protected JarInputStream fetchDistribution(URI uri, HttpURLConnection connection) throws IOException {
        final int status = connection.getResponseCode();

        Log.d(LOGTAG, "Distribution fetch: " + status);
        // We record HTTP statuses as 2xx, 3xx, 4xx, 5xx => 2, 3, 4, 5.
        final int value;
        if (status > 599 || status < 100) {
            Log.wtf(LOGTAG, "Unexpected HTTP status code: " + status);
            value = CODE_CATEGORY_STATUS_OUT_OF_RANGE;
        } else {
            value = status / 100;
        }
        
        Telemetry.addToHistogram(HISTOGRAM_CODE_CATEGORY, value);

        if (status != 200) {
            Log.w(LOGTAG, "Got status " + status + " fetching distribution.");
            Telemetry.addToHistogram(HISTOGRAM_CODE_CATEGORY, CODE_CATEGORY_FETCH_NON_SUCCESS_RESPONSE);
            return null;
        }

        final String contentType = connection.getContentType();
        if (contentType == null || !contentType.startsWith(EXPECTED_CONTENT_TYPE)) {
            Log.w(LOGTAG, "Malformed response: invalid Content-Type.");
            Telemetry.addToHistogram(HISTOGRAM_CODE_CATEGORY, CODE_CATEGORY_FETCH_INVALID_CONTENT_TYPE);
            return null;
        }

        return new JarInputStream(new BufferedInputStream(connection.getInputStream()), true);
    }

    private static void recordFetchTelemetry(final Exception exception) {
        if (exception == null) {
            // Should never happen.
            Telemetry.addToHistogram(HISTOGRAM_CODE_CATEGORY, CODE_CATEGORY_FETCH_EXCEPTION);
            return;
        }

        if (exception instanceof UnknownHostException) {
            // Unknown host => we're offline.
            Telemetry.addToHistogram(HISTOGRAM_CODE_CATEGORY, CODE_CATEGORY_OFFLINE);
            return;
        }

        if (exception instanceof SSLException) {
            Telemetry.addToHistogram(HISTOGRAM_CODE_CATEGORY, CODE_CATEGORY_FETCH_SSL_ERROR);
            return;
        }

        if (exception instanceof ProtocolException ||
            exception instanceof SocketException) {
            Telemetry.addToHistogram(HISTOGRAM_CODE_CATEGORY, CODE_CATEGORY_FETCH_SOCKET_ERROR);
            return;
        }

        Telemetry.addToHistogram(HISTOGRAM_CODE_CATEGORY, CODE_CATEGORY_FETCH_EXCEPTION);
    }

    /**
     * @return true if we copied files out of the APK. Sets distributionDir in that case.
     */
    private boolean checkAPKDistribution() {
        try {
            // First, try copying distribution files out of the APK.
            if (copyFiles()) {
                // We always copy to the data dir, and we only copy files from
                // a 'distribution' subdirectory. Track our dist dir now that
                // we know it.
                this.distributionDir = new File(getDataDir(), DISTRIBUTION_PATH);
                return true;
            }
        } catch (IOException e) {
            Log.e(LOGTAG, "Error copying distribution files from APK.", e);
        }
        return false;
    }

    /**
     * @return true if we found a system distribution. Sets distributionDir in that case.
     */
    private boolean checkSystemDistribution() {
        // If there aren't any distribution files in the APK, look in the /system directory.
        final File distDir = getSystemDistributionDir();
        if (distDir.exists()) {
            this.distributionDir = distDir;
            return true;
        }
        return false;
    }

    /**
     * Unpack distribution files from a downloaded jar stream.
     *
     * The caller is responsible for closing the provided stream.
     */
    private boolean copyFilesFromStream(JarInputStream jar) throws FileNotFoundException, IOException {
        final byte[] buffer = new byte[1024];
        boolean distributionSet = false;
        JarEntry entry;
        while ((entry = jar.getNextJarEntry()) != null) {
            final String name = entry.getName();

            if (entry.isDirectory()) {
                // We'll let getDataFile deal with creating the directory hierarchy.
                // Yes, we can do better, but it can wait.
                continue;
            }

            if (!name.startsWith(DISTRIBUTION_PATH)) {
                continue;
            }

            File outFile = getDataFile(name);
            if (outFile == null) {
                continue;
            }

            distributionSet = true;

            writeStream(jar, outFile, entry.getTime(), buffer);
        }

        return distributionSet;
    }

    /**
     * Copies the /distribution folder out of the APK and into the app's data directory.
     * Returns true if distribution files were found and copied.
     */
    private boolean copyFiles() throws IOException {
        final File applicationPackage = new File(packagePath);
        final ZipFile zip = new ZipFile(applicationPackage);

        boolean distributionSet = false;
        try {
            final byte[] buffer = new byte[1024];

            final Enumeration<? extends ZipEntry> zipEntries = zip.entries();
            while (zipEntries.hasMoreElements()) {
                final ZipEntry fileEntry = zipEntries.nextElement();
                final String name = fileEntry.getName();

                if (fileEntry.isDirectory()) {
                    // We'll let getDataFile deal with creating the directory hierarchy.
                    continue;
                }

                if (!name.startsWith(DISTRIBUTION_PATH)) {
                    continue;
                }

                final File outFile = getDataFile(name);
                if (outFile == null) {
                    continue;
                }

                distributionSet = true;

                final InputStream fileStream = zip.getInputStream(fileEntry);
                try {
                    writeStream(fileStream, outFile, fileEntry.getTime(), buffer);
                } finally {
                    fileStream.close();
                }
            }
        } finally {
            zip.close();
        }

        return distributionSet;
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

    /**
     * Return a File instance in the data directory, ensuring
     * that the parent exists.
     *
     * @return null if the parents could not be created.
     */
    private File getDataFile(final String name) {
        File outFile = new File(getDataDir(), name);
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

    private URI getReferredDistribution(ReferrerDescriptor descriptor) {
        final String content = descriptor.content;
        if (content == null) {
            return null;
        }

        // We restrict here to avoid injection attacks. After all,
        // we're downloading a distribution payload based on intent input.
        if (!content.matches("^[a-zA-Z0-9]+$")) {
            Log.e(LOGTAG, "Invalid referrer content: " + content);
            Telemetry.addToHistogram(HISTOGRAM_REFERRER_INVALID, 1);
            return null;
        }

        try {
            return new URI(FETCH_PROTOCOL, FETCH_HOSTNAME, FETCH_PATH + content + FETCH_EXTENSION, null);
        } catch (URISyntaxException e) {
            // This should never occur.
            Log.wtf(LOGTAG, "Invalid URI with content " + content + "!");
            return null;
        }
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
        File copied = new File(getDataDir(), DISTRIBUTION_PATH);
        if (copied.exists()) {
            return this.distributionDir = copied;
        }
        File system = getSystemDistributionDir();
        if (system.exists()) {
            return this.distributionDir = system;
        }
        return null;
    }

    private String getDataDir() {
        return context.getApplicationInfo().dataDir;
    }

    private File getSystemDistributionDir() {
        return new File("/system/" + context.getPackageName() + "/distribution");
    }

    /**
     * The provided <code>ReadyCallback</code> will be queued for execution after
     * the distribution is ready, or queued for immediate execution if the
     * distribution has already been processed.
     *
     * Each <code>ReadyCallback</code> will be executed on the background thread.
     */
    public void addOnDistributionReadyCallback(final ReadyCallback callback) {
        if (state == STATE_UNKNOWN) {
            // Queue for later.
            onDistributionReady.add(callback);
        } else {
            invokeCallbackDelayed(callback);
        }
    }

    /**
     * Run our delayed queue, after a delayed distribution arrives.
     */
    private void runLateReadyQueue() {
        ReadyCallback task;
        while ((task = onLateReady.poll()) != null) {
            invokeLateCallbackDelayed(task);
        }
    }

    /**
     * Execute tasks that wanted to run when we were done loading
     * the distribution.
     */
    private void runReadyQueue() {
        ReadyCallback task;
        while ((task = onDistributionReady.poll()) != null) {
            invokeCallbackDelayed(task);
        }
    }

    private void invokeLateCallbackDelayed(final ReadyCallback callback) {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                // Sanity.
                if (state != STATE_SET) {
                    Log.w(LOGTAG, "Refusing to invoke late distro callback in state " + state);
                    return;
                }
                callback.distributionArrivedLate(Distribution.this);
            }
        });
    }

    private void invokeCallbackDelayed(final ReadyCallback callback) {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                switch (state) {
                    case STATE_SET:
                        callback.distributionFound(Distribution.this);
                        break;
                    case STATE_NONE:
                        callback.distributionNotFound();
                        if (shouldDelayLateCallbacks) {
                            onLateReady.add(callback);
                        }
                        break;
                    default:
                        throw new IllegalStateException("Expected STATE_NONE or STATE_SET, got " + state);
                }
            }
        });
    }

    /**
     * A safe way for callers to determine if this Distribution instance
     * represents a real live distribution.
     */
    public boolean exists() {
        return state == STATE_SET;
    }

    private String getKeyName() {
        return context.getPackageName() + ".distribution_state";
    }

    private SharedPreferences getSharedPreferences() {
        final SharedPreferences settings;
        if (prefsBranch == null) {
            settings = GeckoSharedPrefs.forApp(context);
        } else {
            settings = context.getSharedPreferences(prefsBranch, Activity.MODE_PRIVATE);
        }
        return settings;
    }
}
