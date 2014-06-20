/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.io.File;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.nio.charset.Charset;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Hashtable;

import org.mozilla.gecko.GeckoProfileDirectories.NoMozillaDirectoryException;
import org.mozilla.gecko.GeckoProfileDirectories.NoSuchProfileException;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.distribution.Distribution;
import org.mozilla.gecko.mozglue.RobocopTarget;
import org.mozilla.gecko.util.INIParser;
import org.mozilla.gecko.util.INISection;

import android.content.ContentResolver;
import android.content.Context;
import android.text.TextUtils;
import android.util.Log;

public final class GeckoProfile {
    private static final String LOGTAG = "GeckoProfile";

    // Used to "lock" the guest profile, so that we'll always restart in it
    private static final String LOCK_FILE_NAME = ".active_lock";
    public static final String DEFAULT_PROFILE = "default";
    private static final String GUEST_PROFILE = "guest";

    private static HashMap<String, GeckoProfile> sProfileCache = new HashMap<String, GeckoProfile>();
    private static String sDefaultProfileName = null;

    // Caches the guest profile dir.
    private static File sGuestDir = null;
    private static GeckoProfile sGuestProfile = null;

    public static boolean sIsUsingCustomProfile = false;

    private final String mName;
    private final File mMozillaDir;
    private final boolean mIsWebAppProfile;
    private final Context mApplicationContext;

    /**
     * Access to this member should be synchronized to avoid
     * races during creation -- particularly between getDir and GeckoView#init.
     *
     * Not final because this is lazily computed. 
     */
    private File mProfileDir;

    // Caches whether or not a profile is "locked".
    // Only used by the guest profile to determine if it should be reused or
    // deleted on startup.
    // These are volatile for an incremental improvement in thread safety,
    // but this is not a complete solution for concurrency.
    private volatile LockState mLocked = LockState.UNDEFINED;
    private volatile boolean mInGuestMode = false;

    // Constants to cache whether or not a profile is "locked".
    private enum LockState {
        LOCKED,
        UNLOCKED,
        UNDEFINED
    };

    public static GeckoProfile get(Context context) {
        boolean isGeckoApp = false;
        try {
            isGeckoApp = context instanceof GeckoApp;
        } catch (NoClassDefFoundError ex) {}

        if (isGeckoApp) {
            // Check for a cached profile on this context already
            // TODO: We should not be caching profile information on the Activity context
            final GeckoApp geckoApp = (GeckoApp) context;
            if (geckoApp.mProfile != null) {
                return geckoApp.mProfile;
            }
        }

        // If the guest profile exists and is locked, return it
        GeckoProfile guest = GeckoProfile.getGuestProfile(context);
        if (guest != null && guest.locked()) {
            return guest;
        }

        if (isGeckoApp) {
            final GeckoApp geckoApp = (GeckoApp) context;
            String defaultProfileName;
            try {
                defaultProfileName = geckoApp.getDefaultProfileName();
            } catch (NoMozillaDirectoryException e) {
                // If this failed, we're screwed. But there are so many callers that
                // we'll just throw a RuntimeException.
                Log.wtf(LOGTAG, "Unable to get default profile name.", e);
                throw new RuntimeException(e);
            }
            // Otherwise, get the default profile for the Activity.
            return get(context, defaultProfileName);
        }

        return get(context, "");
    }

    public static GeckoProfile get(Context context, String profileName) {
        synchronized (sProfileCache) {
            GeckoProfile profile = sProfileCache.get(profileName);
            if (profile != null)
                return profile;
        }
        return get(context, profileName, (File)null);
    }

    @RobocopTarget
    public static GeckoProfile get(Context context, String profileName, String profilePath) {
        File dir = null;
        if (!TextUtils.isEmpty(profilePath)) {
            dir = new File(profilePath);
            if (!dir.exists() || !dir.isDirectory()) {
                Log.w(LOGTAG, "requested profile directory missing: " + profilePath);
            }
        }
        return get(context, profileName, dir);
    }

    @RobocopTarget
    public static GeckoProfile get(Context context, String profileName, File profileDir) {
        if (context == null) {
            throw new IllegalArgumentException("context must be non-null");
        }

        // if no profile was passed in, look for the default profile listed in profiles.ini
        // if that doesn't exist, look for a profile called 'default'
        if (TextUtils.isEmpty(profileName) && profileDir == null) {
            try {
                profileName = GeckoProfile.getDefaultProfileName(context);
            } catch (NoMozillaDirectoryException e) {
                // We're unable to do anything sane here.
                throw new RuntimeException(e);
            }
        }

        // actually try to look up the profile
        synchronized (sProfileCache) {
            GeckoProfile profile = sProfileCache.get(profileName);
            if (profile == null) {
                try {
                    profile = new GeckoProfile(context, profileName);
                } catch (NoMozillaDirectoryException e) {
                    // We're unable to do anything sane here.
                    throw new RuntimeException(e);
                }
                profile.setDir(profileDir);
                sProfileCache.put(profileName, profile);
            } else {
                profile.setDir(profileDir);
            }
            return profile;
        }
    }

    public static boolean removeProfile(Context context, String profileName) {
        if (profileName == null) {
            Log.w(LOGTAG, "Unable to remove profile: null profile name.");
            return false;
        }

        final boolean success;
        try {
            success = new GeckoProfile(context, profileName).remove();
        } catch (NoMozillaDirectoryException e) {
            Log.w(LOGTAG, "Unable to remove profile: no Mozilla directory.", e);
            return true;
        }

        if (success) {
            // Clear all shared prefs for the given profile.
            GeckoSharedPrefs.forProfileName(context, profileName)
                            .edit().clear().commit();
        }

        return success;
    }

    public static GeckoProfile createGuestProfile(Context context) {
        try {
            removeGuestProfile(context);
            // We need to force the creation of a new guest profile if we want it outside of the normal profile path,
            // otherwise GeckoProfile.getDir will try to be smart and build it for us in the normal profiles dir.
            getGuestDir(context).mkdir();
            GeckoProfile profile = getGuestProfile(context);
            profile.lock();
            return profile;
        } catch (Exception ex) {
            Log.e(LOGTAG, "Error creating guest profile", ex);
        }
        return null;
    }

    public static void leaveGuestSession(Context context) {
        GeckoProfile profile = getGuestProfile(context);
        if (profile != null) {
            profile.unlock();
        }
    }

    private static File getGuestDir(Context context) {
        if (sGuestDir == null) {
            sGuestDir = context.getFileStreamPath("guest");
        }
        return sGuestDir;
    }

    private static GeckoProfile getGuestProfile(Context context) {
        if (sGuestProfile == null) {
            File guestDir = getGuestDir(context);
            if (guestDir.exists()) {
                sGuestProfile = get(context, GUEST_PROFILE, guestDir);
                sGuestProfile.mInGuestMode = true;
            }
        }

        return sGuestProfile;
    }

    public static boolean maybeCleanupGuestProfile(final Context context) {
        final GeckoProfile profile = getGuestProfile(context);

        if (profile == null) {
            return false;
        } else if (!profile.locked()) {
            profile.mInGuestMode = false;

            // If the guest dir exists, but it's unlocked, delete it
            removeGuestProfile(context);

            return true;
        }
        return false;
    }

    private static void removeGuestProfile(Context context) {
        boolean success = false;
        try {
            File guestDir = getGuestDir(context);
            if (guestDir.exists()) {
                success = delete(guestDir);
            }
        } catch (Exception ex) {
            Log.e(LOGTAG, "Error removing guest profile", ex);
        }

        if (success) {
            // Clear all shared prefs for the guest profile.
            GeckoSharedPrefs.forProfileName(context, GUEST_PROFILE)
                            .edit().clear().commit();
        }
    }

    public static boolean delete(File file) throws IOException {
        // Try to do a quick initial delete
        if (file.delete())
            return true;

        if (file.isDirectory()) {
            // If the quick delete failed and this is a dir, recursively delete the contents of the dir
            String files[] = file.list();
            for (String temp : files) {
                File fileDelete = new File(file, temp);
                delete(fileDelete);
            }
        }

        // Even if this is a dir, it should now be empty and delete should work
        return file.delete();
    }

    private GeckoProfile(Context context, String profileName) throws NoMozillaDirectoryException {
        if (TextUtils.isEmpty(profileName)) {
            throw new IllegalArgumentException("Unable to create GeckoProfile for empty profile name.");
        }

        mApplicationContext = context.getApplicationContext();
        mName = profileName;
        mIsWebAppProfile = profileName.startsWith("webapp");
        mMozillaDir = GeckoProfileDirectories.getMozillaDirectory(context);
    }

    // Warning, Changing the lock file state from outside apis will cause this to become out of sync
    public boolean locked() {
        if (mLocked != LockState.UNDEFINED) {
            return mLocked == LockState.LOCKED;
        }

        boolean profileExists;
        synchronized (this) {
            profileExists = mProfileDir != null && mProfileDir.exists();
        }

        // Don't use getDir() as it will create a dir if none exists.
        if (profileExists) {
            File lockFile = new File(mProfileDir, LOCK_FILE_NAME);
            boolean res = lockFile.exists();
            mLocked = res ? LockState.LOCKED : LockState.UNLOCKED;
        } else {
            mLocked = LockState.UNLOCKED;
        }

        return mLocked == LockState.LOCKED;
    }

    public boolean lock() {
        try {
            // If this dir doesn't exist getDir will create it for us
            final File lockFile = new File(getDir(), LOCK_FILE_NAME);
            final boolean result = lockFile.createNewFile();
            if (result) {
                mLocked = LockState.LOCKED;
            } else {
                mLocked = LockState.UNLOCKED;
            }
            return result;
        } catch(IOException ex) {
            Log.e(LOGTAG, "Error locking profile", ex);
        }
        mLocked = LockState.UNLOCKED;
        return false;
    }

    public boolean unlock() {
        final File profileDir;
        synchronized (this) {
            // Don't use getDir() as it will create a dir.
            profileDir = mProfileDir;
        }

        if (profileDir == null || !profileDir.exists()) {
            return true;
        }

        try {
            final File lockFile = new File(profileDir, LOCK_FILE_NAME);
            final boolean result = delete(lockFile);
            if (result) {
                mLocked = LockState.UNLOCKED;
            } else {
                mLocked = LockState.LOCKED;
            }
            return result;
        } catch(IOException ex) {
            Log.e(LOGTAG, "Error unlocking profile", ex);
        }
        mLocked = LockState.LOCKED;
        return false;
    }

    public boolean inGuestMode() {
        return mInGuestMode;
    }

    private void setDir(File dir) {
        if (dir != null && dir.exists() && dir.isDirectory()) {
            synchronized (this) {
                mProfileDir = dir;
            }
        }
    }

    public String getName() {
        return mName;
    }

    public synchronized File getDir() {
        forceCreate();
        return mProfileDir;
    }

    public synchronized GeckoProfile forceCreate() {
        if (mProfileDir != null) {
            return this;
        }

        try {
            // Check if a profile with this name already exists.
            try {
                mProfileDir = findProfileDir();
                Log.d(LOGTAG, "Found profile dir.");
            } catch (NoSuchProfileException noSuchProfile) {
                // If it doesn't exist, create it.
                mProfileDir = createProfileDir();
            }
        } catch (IOException ioe) {
            Log.e(LOGTAG, "Error getting profile dir", ioe);
        }
        return this;
    }

    public File getFile(String aFile) {
        File f = getDir();
        if (f == null)
            return null;

        return new File(f, aFile);
    }

    /**
     * Moves the session file to the backup session file.
     *
     * sessionstore.js should hold the current session, and sessionstore.bak
     * should hold the previous session (where it is used to read the "tabs
     * from last time"). Normally, sessionstore.js is moved to sessionstore.bak
     * on a clean quit, but this doesn't happen if Fennec crashed. Thus, this
     * method should be called after a crash so sessionstore.bak correctly
     * holds the previous session.
     */
    public void moveSessionFile() {
        File sessionFile = getFile("sessionstore.js");
        if (sessionFile != null && sessionFile.exists()) {
            File sessionFileBackup = getFile("sessionstore.bak");
            sessionFile.renameTo(sessionFileBackup);
        }
    }

    /**
     * Get the string from a session file.
     *
     * The session can either be read from sessionstore.js or sessionstore.bak.
     * In general, sessionstore.js holds the current session, and
     * sessionstore.bak holds the previous session.
     *
     * @param readBackup if true, the session is read from sessionstore.bak;
     *                   otherwise, the session is read from sessionstore.js
     *
     * @return the session string
     */
    public String readSessionFile(boolean readBackup) {
        File sessionFile = getFile(readBackup ? "sessionstore.bak" : "sessionstore.js");

        try {
            if (sessionFile != null && sessionFile.exists()) {
                return readFile(sessionFile);
            }
        } catch (IOException ioe) {
            Log.e(LOGTAG, "Unable to read session file", ioe);
        }
        return null;
    }

    public String readFile(String filename) throws IOException {
        File dir = getDir();
        if (dir == null) {
            throw new IOException("No profile directory found");
        }
        File target = new File(dir, filename);
        return readFile(target);
    }

    private String readFile(File target) throws IOException {
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

    private boolean remove() {
        try {
            synchronized (this) {
                final File dir = getDir();
                if (dir.exists()) {
                    delete(dir);
                }

                try {
                    mProfileDir = findProfileDir();
                } catch (NoSuchProfileException noSuchProfile) {
                    // If the profile doesn't exist, there's nothing left for us to do.
                    return false;
                }
            }

            final INIParser parser = GeckoProfileDirectories.getProfilesINI(mMozillaDir);
            final Hashtable<String, INISection> sections = parser.getSections();
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
                        String nextSection = "Profile" + (sectionNumber+1);

                        sections.remove(curSection);

                        while (sections.containsKey(nextSection)) {
                            parser.renameSection(nextSection, curSection);
                            sectionNumber++;

                            curSection = nextSection;
                            nextSection = "Profile" + (sectionNumber+1);
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
        return GeckoProfileDirectories.findProfileDir(mMozillaDir, mName);
    }

    private File createProfileDir() throws IOException {
        INIParser parser = GeckoProfileDirectories.getProfilesINI(mMozillaDir);

        // Salt the name of our requested profile
        String saltedName = GeckoProfileDirectories.saltProfileName(mName);
        File profileDir = new File(mMozillaDir, saltedName);
        while (profileDir.exists()) {
            saltedName = GeckoProfileDirectories.saltProfileName(mName);
            profileDir = new File(mMozillaDir, saltedName);
        }

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

        if (!isDefaultSet && !mIsWebAppProfile) {
            // only set as default if this is the first non-webapp
            // profile we're creating
            profileSection.setProperty("Default", 1);

            // We have no intention of stopping this session. The FIRSTRUN session
            // ends when the browsing session/activity has ended. All events
            // during firstrun will be tagged as FIRSTRUN.
            Telemetry.startUISession(TelemetryContract.Session.FIRSTRUN);
        }

        parser.addSection(profileSection);
        parser.write();

        // Trigger init for non-webapp profiles.
        if (!mIsWebAppProfile) {
            enqueueInitialization();
        }

        // Write out profile creation time, mirroring the logic in nsToolkitProfileService.
        try {
            FileOutputStream stream = new FileOutputStream(profileDir.getAbsolutePath() + File.separator + "times.json");
            OutputStreamWriter writer = new OutputStreamWriter(stream, Charset.forName("UTF-8"));
            try {
                writer.append("{\"created\": " + System.currentTimeMillis() + "}\n");
            } finally {
                writer.close();
            }
        } catch (Exception e) {
            // Best-effort.
            Log.w(LOGTAG, "Couldn't write times.json.", e);
        }

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
    public void enqueueInitialization() {
        Log.i(LOGTAG, "Enqueuing profile init.");
        final Context context = mApplicationContext;

        // Add everything when we're done loading the distribution.
        final Distribution distribution = Distribution.getInstance(context);
        distribution.addOnDistributionReadyCallback(new Runnable() {
            @Override
            public void run() {
                Log.d(LOGTAG, "Running post-distribution task: bookmarks.");

                final ContentResolver cr = context.getContentResolver();

                final int offset = BrowserDB.addDistributionBookmarks(cr, distribution, 0);
                BrowserDB.addDefaultBookmarks(context, cr, offset);
            }
        });
    }
}
