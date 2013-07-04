/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.INIParser;
import org.mozilla.gecko.util.INISection;

import android.content.Context;
import android.text.TextUtils;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.nio.charset.Charset;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Hashtable;

public final class GeckoProfile {
    private static final String LOGTAG = "GeckoProfile";

    private static HashMap<String, GeckoProfile> sProfileCache = new HashMap<String, GeckoProfile>();
    private static String sDefaultProfileName = null;

    private final Context mContext;
    private final String mName;
    private File mMozDir;
    private File mDir;

    static private INIParser getProfilesINI(Context context) {
      File filesDir = context.getFilesDir();
      File mozillaDir = new File(filesDir, "mozilla");
      File profilesIni = new File(mozillaDir, "profiles.ini");
      return new INIParser(profilesIni);
    }

    public static GeckoProfile get(Context context) {
        if (context instanceof GeckoApp)
            return get(context, ((GeckoApp)context).getDefaultProfileName());

        return get(context, "");
    }

    public static GeckoProfile get(Context context, String profileName) {
        synchronized (sProfileCache) {
            GeckoProfile profile = sProfileCache.get(profileName);
            if (profile != null)
                return profile;
        }
        return get(context, profileName, null);
    }

    public static GeckoProfile get(Context context, String profileName, String profilePath) {
        if (context == null) {
            throw new IllegalArgumentException("context must be non-null");
        }

        // if no profile was passed in, look for the default profile listed in profiles.ini
        // if that doesn't exist, look for a profile called 'default'
        if (TextUtils.isEmpty(profileName) && TextUtils.isEmpty(profilePath)) {
            profileName = GeckoProfile.findDefaultProfile(context);
            if (profileName == null)
                profileName = "default";
        }

        // actually try to look up the profile
        synchronized (sProfileCache) {
            GeckoProfile profile = sProfileCache.get(profileName);
            if (profile == null) {
                profile = new GeckoProfile(context, profileName, profilePath);
                sProfileCache.put(profileName, profile);
            } else {
                profile.setDir(profilePath);
            }
            return profile;
        }
    }

    public static File ensureMozillaDirectory(Context context) throws IOException {
        synchronized (context) {
            File filesDir = context.getFilesDir();
            File mozDir = new File(filesDir, "mozilla");
            if (! mozDir.exists()) {
                if (! mozDir.mkdirs()) {
                    throw new IOException("Unable to create mozilla directory at " + mozDir.getAbsolutePath());
                }
            }
            return mozDir;
        }
    }

    public static boolean removeProfile(Context context, String profileName) {
        return new GeckoProfile(context, profileName).remove();
    }

    private GeckoProfile(Context context, String profileName) {
        mContext = context;
        mName = profileName;
    }

    private GeckoProfile(Context context, String profileName, String profilePath) {
        mContext = context;
        mName = profileName;
        setDir(profilePath);
    }

    private void setDir(String profilePath) {
        if (!TextUtils.isEmpty(profilePath)) {
            File dir = new File(profilePath);
            if (dir.exists() && dir.isDirectory()) {
                mDir = dir;
            } else {
                Log.w(LOGTAG, "requested profile directory missing: " + profilePath);
            }
        }
    }

    public String getName() {
        return mName;
    }

    public synchronized File getDir() {
        if (mDir != null) {
            return mDir;
        }

        try {
            // Check for old profiles that may need migration.
            ProfileMigrator profileMigrator = new ProfileMigrator(mContext);
            if (!GeckoApp.sIsUsingCustomProfile &&
                !profileMigrator.isProfileMoved()) {
                Log.i(LOGTAG, "New installation or update, checking for old profiles.");
                profileMigrator.launchMoveProfile();
            }

            // now check if a profile with this name that already exists
            File mozillaDir = ensureMozillaDirectory(mContext);
            mDir = findProfileDir(mozillaDir);
            if (mDir == null) {
                // otherwise create it
                mDir = createProfileDir(mozillaDir);
            } else {
                Log.d(LOGTAG, "Found profile dir: " + mDir.getAbsolutePath());
            }
        } catch (IOException ioe) {
            Log.e(LOGTAG, "Error getting profile dir", ioe);
        }
        return mDir;
    }

    public File getFile(String aFile) {
        File f = getDir();
        if (f == null)
            return null;

        return new File(f, aFile);
    }

    public File getFilesDir() {
        return mContext.getFilesDir();
    }

    /**
     * Determines whether the tabs from the previous session should be
     * automatically restored.
     *
     * sessionstore.js is moved to sessionstore.bak on a clean quit, so if we
     * still have sessionstore.js at startup, that means we were killed
     * uncleanly. This is caused by either 1) a crash, or 2) being killed by
     * android because of memory constraints. Either way, the existence of this
     * file indicates that we'll want to restore the previous session.
     *
     * @return whether the previous session should be restored
     */
    public boolean shouldRestoreSession() {
        File sessionFile = getFile("sessionstore.js");
        return sessionFile != null && sessionFile.exists();
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
            File mozillaDir = ensureMozillaDirectory(mContext);
            mDir = findProfileDir(mozillaDir);
            if (mDir == null)
                return false;

            INIParser parser = getProfilesINI(mContext);

            Hashtable<String, INISection> sections = parser.getSections();
            for (Enumeration<INISection> e = sections.elements(); e.hasMoreElements();) {
                INISection section = e.nextElement();
                String name = section.getStringProperty("Name");

                if (name == null || !name.equals(mName))
                    continue;

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
                    return true;
                }
            }

            parser.write();
            return true;
        } catch (IOException ex) {
            Log.w(LOGTAG, "Failed to remove profile " + mName + ":\n" + ex);
            return false;
        }
    }

    public static String findDefaultProfile(Context context) {
        // Have we read the default profile from the INI already?
        // Changing the default profile requires a restart, so we don't
        // need to worry about runtime changes.
        if (sDefaultProfileName != null) {
            return sDefaultProfileName;
        }

        // Open profiles.ini to find the correct path
        INIParser parser = getProfilesINI(context);

        for (Enumeration<INISection> e = parser.getSections().elements(); e.hasMoreElements();) {
            INISection section = e.nextElement();
            if (section.getIntProperty("Default") == 1) {
                sDefaultProfileName = section.getStringProperty("Name");
                return sDefaultProfileName;
            }
        }

        return null;
    }

    private File findProfileDir(File mozillaDir) {
        // Open profiles.ini to find the correct path
        INIParser parser = getProfilesINI(mContext);

        for (Enumeration<INISection> e = parser.getSections().elements(); e.hasMoreElements();) {
            INISection section = e.nextElement();
            String name = section.getStringProperty("Name");
            if (name != null && name.equals(mName)) {
                if (section.getIntProperty("IsRelative") == 1) {
                    return new File(mozillaDir, section.getStringProperty("Path"));
                }
                return new File(section.getStringProperty("Path"));
            }
        }

        return null;
    }

    private static String saltProfileName(String name) {
        String allowedChars = "abcdefghijklmnopqrstuvwxyz0123456789";
        StringBuilder salt = new StringBuilder(16);
        for (int i = 0; i < 8; i++) {
            salt.append(allowedChars.charAt((int)(Math.random() * allowedChars.length())));
        }
        salt.append('.');
        salt.append(name);
        return salt.toString();
    }

    private File createProfileDir(File mozillaDir) throws IOException {
        INIParser parser = getProfilesINI(mContext);

        // Salt the name of our requested profile
        String saltedName = saltProfileName(mName);
        File profileDir = new File(mozillaDir, saltedName);
        while (profileDir.exists()) {
            saltedName = saltProfileName(mName);
            profileDir = new File(mozillaDir, saltedName);
        }

        // Attempt to create the salted profile dir
        if (! profileDir.mkdirs()) {
            throw new IOException("Unable to create profile at " + profileDir.getAbsolutePath());
        }
        Log.d(LOGTAG, "Created new profile dir at " + profileDir.getAbsolutePath());

        // Now update profiles.ini
        // If this is the first time its created, we also add a General section
        // look for the first profile number that isn't taken yet
        int profileNum = 0;
        while (parser.getSection("Profile" + profileNum) != null) {
            profileNum++;
        }

        INISection profileSection = new INISection("Profile" + profileNum);
        profileSection.setProperty("Name", mName);
        profileSection.setProperty("IsRelative", 1);
        profileSection.setProperty("Path", saltedName);

        if (parser.getSection("General") == null) {
            INISection generalSection = new INISection("General");
            generalSection.setProperty("StartWithLastProfile", 1);
            parser.addSection(generalSection);

            // only set as default if this is the first profile we're creating
            profileSection.setProperty("Default", 1);
        }

        parser.addSection(profileSection);
        parser.write();

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
}
