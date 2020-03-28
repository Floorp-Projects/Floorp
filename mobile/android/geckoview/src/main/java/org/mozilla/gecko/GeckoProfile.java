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

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.GeckoProfileDirectories.NoMozillaDirectoryException;
import org.mozilla.gecko.GeckoProfileDirectories.NoSuchProfileException;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.INIParser;
import org.mozilla.gecko.util.INISection;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.nio.charset.Charset;
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

    // Only tests should need to do this.  We can remove this entirely once we
    // fix Bug 1069687.
    private static volatile boolean sAcceptDirectoryChanges = true;

    public static final String DEFAULT_PROFILE = "default";
    // Profile is using a custom directory outside of the Mozilla directory.
    public static final String CUSTOM_PROFILE = "";

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

    public static GeckoProfile initFromArgs(final Context context, final String args) {
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

    private void setDir(final File dir) {
        if (dir != null && dir.exists() && dir.isDirectory()) {
            synchronized (this) {
                mProfileDir = dir;
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

    protected static String generateNewClientId() {
        return UUID.randomUUID().toString();
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

    private static boolean isCanaryClientId(@Nullable final String clientId) {
        return CANARY_CLIENT_ID.equals(clientId);
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
