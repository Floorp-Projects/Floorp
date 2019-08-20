/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.WorkerThread;
import android.text.TextUtils;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.GeckoProfileDirectories.NoMozillaDirectoryException;
import org.mozilla.gecko.GeckoProfileDirectories.NoSuchProfileException;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.util.FileUtils;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.INIParser;
import org.mozilla.gecko.util.INISection;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.nio.charset.Charset;
import java.util.Enumeration;
import java.util.Hashtable;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class GeckoProfile {
    private static final String LOGTAG = "GeckoProfile";

    // The path in the profile to the file containing the client ID.
    private static final String CLIENT_ID_FILE_PATH = "datareporting/state.json";
    // In the client ID file, the attribute title in the JSON object containing the client ID value.
    private static final String CLIENT_ID_JSON_ATTR = "clientID";
    private static final String HAD_CANARY_CLIENT_ID_JSON_ATTR = "wasCanary";
    // Must match the one from TelemetryUtils.jsm
    private static final String CANARY_CLIENT_ID = "c0ffeec0-ffee-c0ff-eec0-ffeec0ffeec0";

    private static final String TIMES_PATH = "times.json";
    private static final String PROFILE_CREATION_DATE_JSON_ATTR = "created";

    // Only tests should need to do this.  We can remove this entirely once we
    // fix Bug 1069687.
    private static volatile boolean sAcceptDirectoryChanges = true;

    @RobocopTarget
    public static void enableDirectoryChanges() {
        Log.w(LOGTAG, "Directory changes should only be enabled for tests. And even then it's a bad idea.");
        sAcceptDirectoryChanges = true;
    }

    public static final String DEFAULT_PROFILE = "default";
    // Profile is using a custom directory outside of the Mozilla directory.
    public static final String CUSTOM_PROFILE = "";

    public static final String GUEST_PROFILE_DIR = "guest";
    public static final String GUEST_MODE_PREF = "guestMode";

    // Session store
    private static final String SESSION_FILE = "sessionstore.js";
    private static final String SESSION_FILE_BACKUP = "sessionstore.bak";
    private static final String SESSION_FILE_PREVIOUS = "sessionstore.old";
    private static final long MAX_PREVIOUS_FILE_AGE = 1000 * 3600 * 24; // 24 hours

    private boolean mOldSessionDataProcessed = false;

    private static final ConcurrentHashMap<String, GeckoProfile> sProfileCache =
            new ConcurrentHashMap<String, GeckoProfile>(
                    /* capacity */ 4, /* load factor */ 0.75f, /* concurrency */ 2);
    private static String sDefaultProfileName;
    private static String sIntentArgs;

    private final String mName;
    private final File mMozillaDir;

    private Object mData;

    /**
     * Access to this member should be synchronized to avoid
     * races during creation -- particularly between getDir and GeckoView#init.
     *
     * Not final because this is lazily computed.
     */
    private File mProfileDir;

    private Boolean mInGuestMode;

    public static boolean shouldUseGuestMode(final Context context) {
        return GeckoSharedPrefs.forApp(context).getBoolean(GUEST_MODE_PREF, false);
    }

    public static void enterGuestMode(final Context context) {
        GeckoSharedPrefs.forApp(context).edit().putBoolean(GUEST_MODE_PREF, true).commit();
    }

    public static void leaveGuestMode(final Context context) {
        GeckoSharedPrefs.forApp(context).edit().putBoolean(GUEST_MODE_PREF, false).commit();
    }

    public static void setIntentArgs(final String intentArgs) {
        sIntentArgs = intentArgs;
    }

    public static GeckoProfile initFromArgs(final Context context, final String args) {
        if (shouldUseGuestMode(context)) {
            final GeckoProfile guestProfile = getGuestProfile(context);
            if (guestProfile != null) {
                return guestProfile;
            }
            // Failed to create guest profile; leave guest mode.
            leaveGuestMode(context);
        }

        // We never want to use the guest mode profile concurrently with a normal profile
        // -- no syncing to it, no dual-profile usage, nothing.  GeckoThread startup with
        // a conventional GeckoProfile will cause the guest profile to be deleted and
        // guest mode to reset.
        if (getGuestDir(context).isDirectory()) {
            final GeckoProfile guestProfile = getGuestProfile(context);
            if (guestProfile != null) {
                removeProfile(context, guestProfile);
            }
        }

        String profileName = null;
        String profilePath = null;

        if (args != null && args.contains("-P")) {
            final Pattern p = Pattern.compile("(?:-P\\s*)(\\w*)(\\s*)");
            final Matcher m = p.matcher(args);
            if (m.find()) {
                profileName = m.group(1);
            }
        }

        if (args != null && args.contains("-profile")) {
            final Pattern p = Pattern.compile("(?:-profile\\s*)(\\S*)(\\s*)");
            final Matcher m = p.matcher(args);
            if (m.find()) {
                profilePath =  m.group(1);
            }
        }

        if (TextUtils.isEmpty(profileName) && profilePath == null) {
            informIfCustomProfileIsUnavailable(profileName, false);
            // Get the default profile for the Activity.
            return getDefaultProfile(context);
        }

        return GeckoProfile.get(context, profileName, profilePath);
    }

    private static GeckoProfile getDefaultProfile(final Context context) {
        try {
            return get(context, getDefaultProfileName(context));

        } catch (final NoMozillaDirectoryException e) {
            // If this failed, we're screwed.
            Log.wtf(LOGTAG, "Unable to get default profile name.", e);
            throw new RuntimeException(e);
        }
    }

    public static GeckoProfile get(final Context context) {
        return get(context, null, (File) null);
    }

    public static GeckoProfile get(final Context context, final String profileName) {
        if (profileName != null) {
            GeckoProfile profile = sProfileCache.get(profileName);
            if (profile != null)
                return profile;
        }
        return get(context, profileName, (File)null);
    }

    @RobocopTarget
    public static GeckoProfile get(final Context context, final String profileName,
                                   final String profilePath) {
        File dir = null;
        if (!TextUtils.isEmpty(profilePath)) {
            dir = new File(profilePath);
            if (!dir.exists() || !dir.isDirectory()) {
                Log.w(LOGTAG, "requested profile directory missing: " + profilePath);
            }
        }
        return get(context, profileName, dir);
    }

    // Note that the profile cache respects only the profile name!
    // If the directory changes, the returned GeckoProfile instance will be mutated.
    @RobocopTarget
    public static GeckoProfile get(final Context context, final String profileName,
                                   final File profileDir) {
        if (context == null) {
            throw new IllegalArgumentException("context must be non-null");
        }

        // Null name? | Null dir? | Returned profile
        // ------------------------------------------
        //     Yes    |    Yes    | Active profile or default profile.
        //     No     |    Yes    | Profile with specified name at default dir.
        //     Yes    |    No     | Custom (anonymous) profile with specified dir.
        //     No     |    No     | Profile with specified name at specified dir.
        //
        // Empty name?| Null dir? | Returned profile
        // ------------------------------------------
        //     Yes    |    Yes    | Active profile or default profile

        String resolvedProfileName = profileName;
        if (TextUtils.isEmpty(profileName) && profileDir == null) {
            // If no profile info was passed in, look for the active profile or a default profile.
            final GeckoProfile profile = GeckoThread.getActiveProfile();
            if (profile != null) {
                informIfCustomProfileIsUnavailable(profileName, true);
                return profile;
            }

            informIfCustomProfileIsUnavailable(profileName, false);
            return GeckoProfile.initFromArgs(context, sIntentArgs);
        } else if (profileName == null) {
            // If only profile dir was passed in, use custom (anonymous) profile.
            resolvedProfileName = CUSTOM_PROFILE;
        }

        // We require the profile dir to exist if specified, so create it here if needed.
        final boolean init = profileDir != null && profileDir.mkdirs();
        if (init) {
            Log.d(LOGTAG, "Creating profile directory: " + profileDir);
        }

        // Actually try to look up the profile.
        GeckoProfile profile = sProfileCache.get(resolvedProfileName);
        GeckoProfile newProfile = null;

        if (profile == null) {
            try {
                Log.d(LOGTAG, "Loading profile at: " + profileDir + " name: " + resolvedProfileName);
                newProfile = new GeckoProfile(context, resolvedProfileName, profileDir);
            } catch (NoMozillaDirectoryException e) {
                // We're unable to do anything sane here.
                throw new RuntimeException(e);
            }

            profile = sProfileCache.putIfAbsent(resolvedProfileName, newProfile);
        }

        if (profile == null) {
            profile = newProfile;

        } else if (profileDir != null) {
            // We have an existing profile but was given an alternate directory.
            boolean consistent = false;
            try {
                consistent = profile.mProfileDir != null &&
                        profile.mProfileDir.getCanonicalPath().equals(profileDir.getCanonicalPath());
            } catch (final IOException e) {
            }

            if (!consistent) {
                if (!sAcceptDirectoryChanges || !profileDir.isDirectory()) {
                    throw new IllegalStateException(
                            "Refusing to reuse profile with a different directory.");
                }
                profile.setDir(profileDir);
            }
        }

        if (init) {
            // Initialize the profile directory if we had to create it.
            profile.enqueueInitialization(profileDir);
        }

        return profile;
    }

    /**
     * Custom profiles are an edge use case (must be passed in via Intent arguments)<br>
     * Will inform users if the received arguments are invalid and the app fallbacks to use
     * the currently active or the default Gecko profile.<br>
     * Only to be called if other conditions than the profile name are already checked.
     *
     * @see <a href="http://google.com">Reasoning behind custom profiles</a>
     *
     * @param profileName intended profile name. Will be checked against {{@link #CUSTOM_PROFILE}}
     *                    to decide if we should inform or not about using the fallback profile.
     * @param activeOrDefaultProfileFallback true - will fallback to use the currently active Gecko profile
     *                                       false - will fallback to use the default Gecko profile
     */
    private static void informIfCustomProfileIsUnavailable(
            final String profileName, final boolean activeOrDefaultProfileFallback) {
        if (CUSTOM_PROFILE.equals(profileName)) {
            final String fallbackProfileName = activeOrDefaultProfileFallback ? "active" : "default";
            Log.w(LOGTAG, String.format("Custom profile must have a directory specified! " +
                    "Reverting to use the %s profile", fallbackProfileName));
        }
    }

    // Currently unused outside of testing.
    @RobocopTarget
    public static boolean removeProfile(final Context context, final GeckoProfile profile) {
        final boolean success = profile.remove();

        if (success) {
            // Clear all shared prefs for the given profile.
            GeckoSharedPrefs.forProfileName(context, profile.getName())
                            .edit().clear().apply();
        }

        return success;
    }

    private static File getGuestDir(final Context context) {
        return context.getFileStreamPath(GUEST_PROFILE_DIR);
    }

    @RobocopTarget
    public static GeckoProfile getGuestProfile(final Context context) {
        return get(context, CUSTOM_PROFILE, getGuestDir(context));
    }

    public static boolean isGuestProfile(final Context context, final String profileName,
                                         final File profileDir) {
        // Guest profile is just a custom profile with a special path.
        if (profileDir == null || !CUSTOM_PROFILE.equals(profileName)) {
            return false;
        }

        try {
            return profileDir.getCanonicalPath().equals(getGuestDir(context).getCanonicalPath());
        } catch (final IOException e) {
            return false;
        }
    }

    private GeckoProfile(final Context context, final String profileName, final File profileDir)
            throws NoMozillaDirectoryException {
        if (profileName == null) {
            throw new IllegalArgumentException("Unable to create GeckoProfile for empty profile name.");
        }

        mName = profileName;
        mMozillaDir = GeckoProfileDirectories.getMozillaDirectory(context);

        mProfileDir = profileDir;
        if (profileDir != null) {
            if (!profileDir.isDirectory()) {
                throw new IllegalArgumentException("Profile directory must exist if specified: " +
                        profileDir.getPath());
            }

            // Ensure that we can write to the profile directory.
            //
            // We would use `writeFile`, but that function just logs exceptions; we need them to
            // provide useful feedback.
            FileWriter fileWriter = null;
            try {
                fileWriter = new FileWriter(new File(profileDir, ".can-write-sentinel"), false);
                fileWriter.write(0);
            } catch (IOException e) {
                throw new IllegalArgumentException("Profile directory must be writable if specified: " +
                        profileDir.getPath(), e);
            } finally {
                try {
                    if (fileWriter != null) {
                        fileWriter.close();
                    }
                } catch (IOException e) {
                    Log.e(LOGTAG, "Error closing .can-write-sentinel; ignoring", e);
                }
            }
        }
    }

    /**
     * Return the custom data object associated with this profile, which was set by the
     * previous {@link #setData(Object)} call. This association is valid for the duration
     * of the process lifetime. The caller must ensure proper synchronization, typically
     * by synchronizing on the object returned by {@link #getLock()}.
     *
     * The data object is usually a database object that stores per-profile data such as
     * page history. However, it can be any other object that needs to maintain
     * profile-specific state.
     *
     * @return Associated data object
     */
    public Object getData() {
        return mData;
    }

    /**
     * Associate this profile with a custom data object, which can be retrieved by
     * subsequent {@link #getData()} calls. The caller must ensure proper
     * synchronization, typically by synchronizing on the object returned by {@link
     * #getLock()}.
     *
     * @param data Custom data object
     */
    public void setData(final Object data) {
        mData = data;
    }

    private void setDir(final File dir) {
        if (dir != null && dir.exists() && dir.isDirectory()) {
            synchronized (this) {
                mProfileDir = dir;
                mInGuestMode = null;
            }
        }
    }

    @RobocopTarget
    public String getName() {
        return mName;
    }

    public boolean isCustomProfile() {
        return CUSTOM_PROFILE.equals(mName);
    }

    @RobocopTarget
    public boolean inGuestMode() {
        if (mInGuestMode == null) {
            mInGuestMode = isGuestProfile(GeckoAppShell.getApplicationContext(),
                                          mName, mProfileDir);
        }
        return mInGuestMode;
    }

    /**
     * Return an Object that can be used with a synchronized statement to allow
     * exclusive access to the profile.
     */
    public Object getLock() {
        return this;
    }

    /**
     * Retrieves the directory backing the profile. This method acts
     * as a lazy initializer for the GeckoProfile instance.
     */
    @RobocopTarget
    public synchronized File getDir() {
        forceCreateLocked();
        return mProfileDir;
    }

    /**
     * Forces profile creation. Consider using {@link #getDir()} to initialize the profile instead - it is the
     * lazy initializer and, for our code reasoning abilities, we should initialize the profile in one place.
     */
    private void forceCreateLocked() {
        if (mProfileDir != null) {
            return;
        }

        try {
            // Check if a profile with this name already exists.
            try {
                mProfileDir = findProfileDir();
                Log.d(LOGTAG, "Found profile dir: " + mProfileDir);
            } catch (NoSuchProfileException noSuchProfile) {
                // If it doesn't exist, create it.
                mProfileDir = createProfileDir();
                Log.d(LOGTAG, "Creating profile dir: " + mProfileDir);
            }
        } catch (IOException ioe) {
            Log.e(LOGTAG, "Error getting profile dir", ioe);
        }
    }

    public File getFile(final String aFile) {
        File f = getDir();
        if (f == null)
            return null;

        return new File(f, aFile);
    }

    /**
     * Retrieves the Gecko client ID from the filesystem. If the client ID does not exist,
     * we attempt to create a new one ourselves.
     *
     * This method assumes the client ID is located in a file at a hard-coded path within the profile. The format of
     * this file is a JSONObject which at the bottom level contains a String -&gt; String mapping containing the client ID.
     *
     * WARNING: the platform provides a JSM to retrieve the client ID [1] and this would be a
     * robust way to access it. However, we don't want to rely on Gecko running in order to get
     * the client ID so instead we access the file this module accesses directly. However, it's
     * possible the format of this file (and the access calls in the jsm) will change, leaving
     * this code to fail. There are tests in TestGeckoProfile to verify the file format but be
     * warned: THIS IS NOT FOOLPROOF.
     *
     * [1]: https://dxr.mozilla.org/mozilla-central/source/toolkit/modules/ClientID.jsm
     *
     * @throws IOException if the client ID could not be retrieved.
     */
    // Mimics ClientID.jsm - _doLoadClientID.
    @WorkerThread
    public String getClientId() throws IOException {
        String clientId = "";
        try {
            clientId = getClientIdFromDisk(CLIENT_ID_FILE_PATH);
        } catch (final IOException e) {
            // Avoid log spam: don't log the full Exception w/ the stack trace.
            Log.d(LOGTAG, "Could not get client ID - creating a new one: " + e.getLocalizedMessage());
        }

        if (isClientIdValid(clientId)) {
            return clientId;
        } else {
            String newClientId = generateNewClientId();
            // There is a possibility Gecko is running and the Gecko telemetry implementation decided it's time to generate
            // the client ID, writing client ID underneath us. Since it's highly unlikely (e.g. we run in onStart before
            // Gecko is started), we don't handle that possibility besides writing the ID and then reading from the file
            // again (rather than just returning the value we generated before writing).
            //
            // In the event it does happen, any discrepancy will be resolved after a restart. In the mean time, both this
            // implementation and the Gecko implementation could upload documents with inconsistent IDs.
            //
            // In any case, if we get an exception, intentionally throw - there's nothing more to do here.
            persistNewClientId(clientId, newClientId);
        }

        return getClientIdFromDisk(CLIENT_ID_FILE_PATH);
    }

    @WorkerThread
    public boolean getIfHadCanaryClientId() throws IOException {
        final JSONObject obj = readJSONObjectFromFile(CLIENT_ID_FILE_PATH);
        return obj.optBoolean(HAD_CANARY_CLIENT_ID_JSON_ATTR);
    }

    protected static String generateNewClientId() {
        return UUID.randomUUID().toString();
    }

    /**
     * @return a valid client ID
     * @throws IOException if a valid client ID could not be retrieved
     */
    @WorkerThread
    private String getClientIdFromDisk(final String filePath) throws IOException {
        final JSONObject obj = readJSONObjectFromFile(filePath);
        return obj.optString(CLIENT_ID_JSON_ATTR);
    }

    /**
     * Persists the given client ID to disk. This will overwrite any existing files.
     */
    @WorkerThread
    private void persistNewClientId(@Nullable final String oldClientId,
                                    @NonNull final String newClientId) throws IOException {
        if (!ensureParentDirs(CLIENT_ID_FILE_PATH)) {
            throw new IOException("Could not create client ID parent directories");
        }

        final JSONObject obj = new JSONObject();
        try {
            obj.put(CLIENT_ID_JSON_ATTR, newClientId);
            obj.put(HAD_CANARY_CLIENT_ID_JSON_ATTR, isCanaryClientId(oldClientId));
        } catch (final JSONException e) {
            throw new IOException("Could not create client ID JSON object", e);
        }

        // ClientID.jsm overwrites the file to store the client ID so it's okay if we do it too.
        Log.d(LOGTAG, "Attempting to write new client ID properties");
        writeFile(CLIENT_ID_FILE_PATH, obj.toString()); // Logs errors within function: ideally we'd throw.
    }

    // From ClientID.jsm - isValidClientID.
    public static boolean isClientIdValid(@Nullable final String clientId) {
        // We could use UUID.fromString but, for consistency, we take the implementation from ClientID.jsm.
        if (TextUtils.isEmpty(clientId)) {
            return false;
        }

        if (CANARY_CLIENT_ID.equals(clientId)) {
            return false;
        }

        return clientId.matches("(?i:[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})");
    }

    private static boolean isCanaryClientId(@Nullable final String clientId) {
        return CANARY_CLIENT_ID.equals(clientId);
    }

    /**
     * Gets the profile creation date and persists it if it had to be generated.
     *
     * To get this value, we first look in times.json. If that could not be accessed, we
     * return the package's first install date. This is not a perfect solution because a
     * user may have large gap between install time and first use.
     *
     * A more correct algorithm could be the one performed by the JS code in ProfileAge.jsm
     * getOldestProfileTimestamp: walk the tree and return the oldest timestamp on the files
     * within the profile. However, since times.json will only not exist for the small
     * number of really old profiles, we're okay with the package install date compromise for
     * simplicity.
     *
     * @return the profile creation date in the format returned by {@link System#currentTimeMillis()}
     *         or -1 if the value could not be persisted.
     */
    @WorkerThread
    public long getAndPersistProfileCreationDate(final Context context) {
        try {
            return getProfileCreationDateFromTimesFile();
        } catch (final IOException e) {
            Log.d(LOGTAG, "Unable to retrieve profile creation date from times.json. Getting from system...");
            final long packageInstallMillis = org.mozilla.gecko.util.ContextUtils.getCurrentPackageInfo(context).firstInstallTime;
            try {
                persistProfileCreationDateToTimesFile(packageInstallMillis);
            } catch (final IOException ioEx) {
                // We return -1 to ensure the profileCreationDate
                // will either be an error (-1) or a consistent value.
                Log.w(LOGTAG, "Unable to persist profile creation date - returning -1");
                return -1;
            }

            return packageInstallMillis;
        }
    }

    @WorkerThread
    private long getProfileCreationDateFromTimesFile() throws IOException {
        final JSONObject obj = readJSONObjectFromFile(TIMES_PATH);
        try {
            return obj.getLong(PROFILE_CREATION_DATE_JSON_ATTR);
        } catch (final JSONException e) {
            // Don't log to avoid leaking data in JSONObject.
            throw new IOException("Profile creation does not exist in JSONObject");
        }
    }

    @WorkerThread
    private void persistProfileCreationDateToTimesFile(final long profileCreationMillis) throws IOException {
        final JSONObject obj = new JSONObject();
        try {
            obj.put(PROFILE_CREATION_DATE_JSON_ATTR, profileCreationMillis);
        } catch (final JSONException e) {
            // Don't log to avoid leaking data in JSONObject.
            throw new IOException("Unable to persist profile creation date to times file");
        }
        Log.d(LOGTAG, "Attempting to write new profile creation date");
        writeFile(TIMES_PATH, obj.toString()); // Ideally we'd throw here too.
    }

    /**
     * Updates the state of the old session data file.
     *
     * sessionstore.js should hold the current session, and sessionstore.old should
     * hold the previous session (where it is used to read the "tabs from last time").
     * If we're not restoring tabs automatically, sessionstore.js needs to be moved to
     * sessionstore.old, so we can display the correct "tabs from last time".
     * If we *are* restoring tabs, we need to delete outdated copies of sessionstore.old,
     * so we don't continue showing stale "tabs from last time" indefinitely.
     *
     * @param shouldRestore Pass true if we are automatically restoring last session's tabs.
     */
    public void updateSessionFile(final boolean shouldRestore) {
        File sessionFilePrevious = getFile(SESSION_FILE_PREVIOUS);
        if (!shouldRestore) {
            File sessionFile = getFile(SESSION_FILE);
            if (sessionFile != null && sessionFile.exists()) {
                sessionFile.renameTo(sessionFilePrevious);
            }
        } else {
            if (sessionFilePrevious != null && sessionFilePrevious.exists() &&
                    System.currentTimeMillis() - sessionFilePrevious.lastModified() > MAX_PREVIOUS_FILE_AGE) {
                sessionFilePrevious.delete();
            }
        }
        synchronized (this) {
            mOldSessionDataProcessed = true;
            notifyAll();
        }
    }

    public void waitForOldSessionDataProcessing() {
        synchronized (this) {
            while (!mOldSessionDataProcessed) {
                try {
                    wait();
                } catch (final InterruptedException e) {
                    // Ignore and wait again.
                }
            }
        }
    }

    /**
     * Get the string from a session file.
     *
     * The session can either be read from sessionstore.js or sessionstore.bak.
     * In general, sessionstore.js holds the current session, and
     * sessionstore.bak holds a backup copy in case of interrupted writes.
     *
     * @param readBackup if true, the session is read from sessionstore.bak;
     *                   otherwise, the session is read from sessionstore.js
     *
     * @return the session string
     */
    public String readSessionFile(final boolean readBackup) {
        return readSessionFile(readBackup ? SESSION_FILE_BACKUP : SESSION_FILE);
    }

    /**
     * Get the string from last session's session file.
     *
     * If we are not restoring tabs automatically, sessionstore.old will contain
     * the previous session.
     *
     * @return the session string
     */
    public String readPreviousSessionFile() {
        return readSessionFile(SESSION_FILE_PREVIOUS);
    }

    private String readSessionFile(final String fileName) {
        File sessionFile = getFile(fileName);

        try {
            if (sessionFile != null && sessionFile.exists()) {
                return readFile(sessionFile);
            }
        } catch (IOException ioe) {
            Log.e(LOGTAG, "Unable to read session file", ioe);
        }
        return null;
    }

    /**
     * Checks whether the session store file exists.
     */
    public boolean sessionFileExists() {
        File sessionFile = getFile(SESSION_FILE);

        return sessionFile != null && sessionFile.exists();
    }

    /**
     * Ensures the parent director(y|ies) of the given filename exist by making them
     * if they don't already exist..
     *
     * @param filename The path to the file whose parents should be made directories
     * @return true if the parent directory exists, false otherwise
     */
    @WorkerThread
    protected boolean ensureParentDirs(final String filename) {
        final File file = new File(getDir(), filename);
        final File parentFile = file.getParentFile();
        return parentFile.mkdirs() || parentFile.isDirectory();
    }

    public void writeFile(final String filename, final String data) {
        File file = new File(getDir(), filename);
        BufferedWriter bufferedWriter = null;
        try {
            bufferedWriter = new BufferedWriter(new FileWriter(file, false));
            bufferedWriter.write(data);
        } catch (IOException e) {
            Log.e(LOGTAG, "Unable to write to file", e);
        } finally {
            try {
                if (bufferedWriter != null) {
                    bufferedWriter.close();
                }
            } catch (IOException e) {
                Log.e(LOGTAG, "Error closing writer while writing to file", e);
            }
        }
    }

    @WorkerThread
    public JSONObject readJSONObjectFromFile(final String filename) throws IOException {
        final String fileContents;
        try {
            fileContents = readFile(filename);
        } catch (final IOException e) {
            // Don't log exception to avoid leaking profile path.
            throw new IOException("Could not access given file to retrieve JSONObject");
        }

        try {
            return new JSONObject(fileContents);
        } catch (final JSONException e) {
            // Don't log exception to avoid leaking profile path.
            throw new IOException("Could not parse JSON to retrieve JSONObject");
        }
    }

    public JSONArray readJSONArrayFromFile(final String filename) {
        String fileContent;
        try {
            fileContent = readFile(filename);
        } catch (IOException expected) {
            return new JSONArray();
        }

        JSONArray jsonArray;
        try {
            jsonArray = new JSONArray(fileContent);
        } catch (JSONException e) {
            jsonArray = new JSONArray();
        }
        return jsonArray;
    }

    public String readFile(final String filename) throws IOException {
        File dir = getDir();
        if (dir == null) {
            throw new IOException("No profile directory found");
        }
        File target = new File(dir, filename);
        return readFile(target);
    }

    private String readFile(final File target) throws IOException {
        FileReader fr = new FileReader(target);
        try {
            StringBuilder sb = new StringBuilder();
            char[] buf = new char[8192];
            int read = fr.read(buf);
            while (read >= 0) {
                sb.append(buf, 0, read);
                read = fr.read(buf);
            }
            return sb.toString();
        } finally {
            fr.close();
        }
    }

    public boolean deleteFileFromProfileDir(final String fileName) throws IllegalArgumentException {
        if (TextUtils.isEmpty(fileName)) {
            throw new IllegalArgumentException("Filename cannot be empty.");
        }
        File file = new File(getDir(), fileName);
        return file.delete();
    }

    private boolean remove() {
        try {
            synchronized (this) {
                if (mProfileDir != null && mProfileDir.exists()) {
                    FileUtils.delete(mProfileDir);
                }

                if (isCustomProfile()) {
                    // Custom profiles don't have profile.ini sections that we need to remove.
                    return true;
                }

                try {
                    // If findProfileDir() succeeds, it means the profile was created
                    // through forceCreate(), so we set mProfileDir to null to enable
                    // forceCreate() to create the profile again.
                    findProfileDir();
                    mProfileDir = null;

                } catch (final NoSuchProfileException e) {
                    // If findProfileDir() throws, it means the profile was not created
                    // through forceCreate(), and we have to preserve mProfileDir because
                    // it was given to us. In that case, there's nothing left to do here.
                    return true;
                }
            }

            final INIParser parser = GeckoProfileDirectories.getProfilesINI(mMozillaDir);
            final Hashtable<String, INISection> sections = parser.getSections();
            if (sections == null) {
                return false;
            }
            for (Enumeration<INISection> e = sections.elements(); e.hasMoreElements();) {
                final INISection section = e.nextElement();
                String name = section.getStringProperty("Name");

                if (name == null || !name.equals(mName)) {
                    continue;
                }

                if (section.getName().startsWith("Profile")) {
                    // ok, we have stupid Profile#-named things.  Rename backwards.
                    try {
                        int sectionNumber = Integer.parseInt(section.getName().substring("Profile".length()));
                        String curSection = "Profile" + sectionNumber;
                        String nextSection = "Profile" + (sectionNumber + 1);

                        sections.remove(curSection);

                        while (sections.containsKey(nextSection)) {
                            parser.renameSection(nextSection, curSection);
                            sectionNumber++;

                            curSection = nextSection;
                            nextSection = "Profile" + (sectionNumber + 1);
                        }
                    } catch (NumberFormatException nex) {
                        // uhm, malformed Profile thing; we can't do much.
                        Log.e(LOGTAG, "Malformed section name in profiles.ini: " + section.getName());
                        return false;
                    }
                } else {
                    // this really shouldn't be the case, but handle it anyway
                    parser.removeSection(mName);
                }

                break;
            }

            parser.write();
            return true;
        } catch (IOException ex) {
            Log.w(LOGTAG, "Failed to remove profile.", ex);
            return false;
        }
    }

    /**
     * @return the default profile name for this application, or
     *         {@link GeckoProfile#DEFAULT_PROFILE} if none could be found.
     *
     * @throws NoMozillaDirectoryException
     *             if the Mozilla directory did not exist and could not be
     *             created.
     */
    public static String getDefaultProfileName(final Context context) throws NoMozillaDirectoryException {
        // Have we read the default profile from the INI already?
        // Changing the default profile requires a restart, so we don't
        // need to worry about runtime changes.
        if (sDefaultProfileName != null) {
            return sDefaultProfileName;
        }

        final String profileName = GeckoProfileDirectories.findDefaultProfileName(context);
        if (profileName == null) {
            // Note that we don't persist this back to profiles.ini.
            sDefaultProfileName = DEFAULT_PROFILE;
            return DEFAULT_PROFILE;
        }

        sDefaultProfileName = profileName;
        return sDefaultProfileName;
    }

    private File findProfileDir() throws NoSuchProfileException {
        if (isCustomProfile()) {
            return mProfileDir;
        }
        return GeckoProfileDirectories.findProfileDir(mMozillaDir, mName);
    }

    @WorkerThread
    private File createProfileDir() throws IOException {
        if (isCustomProfile()) {
            // Custom profiles must already exist.
            return mProfileDir;
        }

        INIParser parser = GeckoProfileDirectories.getProfilesINI(mMozillaDir);

        // Salt the name of our requested profile
        String saltedName;
        File profileDir;
        do {
            saltedName = GeckoProfileDirectories.saltProfileName(mName);
            profileDir = new File(mMozillaDir, saltedName);
        } while (profileDir.exists());

        // Attempt to create the salted profile dir
        if (!profileDir.mkdirs()) {
            throw new IOException("Unable to create profile.");
        }
        Log.d(LOGTAG, "Created new profile dir.");

        // Now update profiles.ini
        // If this is the first time its created, we also add a General section
        // look for the first profile number that isn't taken yet
        int profileNum = 0;
        boolean isDefaultSet = false;
        INISection profileSection;
        while ((profileSection = parser.getSection("Profile" + profileNum)) != null) {
            profileNum++;
            if (profileSection.getProperty("Default") != null) {
                isDefaultSet = true;
            }
        }

        profileSection = new INISection("Profile" + profileNum);
        profileSection.setProperty("Name", mName);
        profileSection.setProperty("IsRelative", 1);
        profileSection.setProperty("Path", saltedName);

        if (parser.getSection("General") == null) {
            INISection generalSection = new INISection("General");
            generalSection.setProperty("StartWithLastProfile", 1);
            parser.addSection(generalSection);
        }

        if (!isDefaultSet) {
            // only set as default if this is the first profile we're creating
            profileSection.setProperty("Default", 1);
        }

        parser.addSection(profileSection);
        parser.write();

        enqueueInitialization(profileDir);

        // Write out profile creation time, mirroring the logic in nsToolkitProfileService.
        try {
            FileOutputStream stream = new FileOutputStream(profileDir.getAbsolutePath() + File.separator + TIMES_PATH);
            OutputStreamWriter writer = new OutputStreamWriter(stream, Charset.forName("UTF-8"));
            try {
                writer.append("{\"created\": " + System.currentTimeMillis() + "}\n");
            } finally {
                writer.close();
            }
        } catch (Exception e) {
            // Best-effort.
            Log.w(LOGTAG, "Couldn't write " + TIMES_PATH, e);
        }

        // Create the client ID file before Gecko starts (we assume this method
        // is called before Gecko starts). If we let Gecko start, the JS telemetry
        // code may try to write to the file at the same time Java does.
        persistNewClientId(null, generateNewClientId());

        return profileDir;
    }

    /**
     * This method is called once, immediately before creation of the profile
     * directory completes.
     *
     * It queues up work to be done in the background to prepare the profile,
     * such as adding default bookmarks.
     *
     * This is public for use *from tests only*!
     */
    @RobocopTarget
    public void enqueueInitialization(final File profileDir) {
        Log.i(LOGTAG, "Enqueuing profile init.");

        final GeckoBundle message = new GeckoBundle(2);
        message.putString("name", getName());
        message.putString("path", profileDir.getAbsolutePath());
        EventDispatcher.getInstance().dispatch("Profile:Create", message);
    }
}
