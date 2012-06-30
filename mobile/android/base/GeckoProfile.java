/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.nio.channels.FileChannel;
import java.util.HashMap;
import java.util.Hashtable;
import java.util.Enumeration;
import android.content.Context;
import android.os.Build;
import android.os.SystemClock;
import android.text.TextUtils;
import android.util.Log;

public final class GeckoProfile {
    private static final String LOGTAG = "GeckoProfile";

    private static HashMap<String, GeckoProfile> sProfileCache = new HashMap<String, GeckoProfile>();

    private final Context mContext;
    private final String mName;
    private File mMozDir;
    private File mDir;

    // this short timeout is a temporary fix until bug 735399 is implemented
    private static final long SESSION_TIMEOUT = 30 * 1000; // 30 seconds

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
        return get(context, profileName, null);
    }

    public static GeckoProfile get(Context context, String profileName, String profilePath) {
        if (context == null) {
            throw new IllegalArgumentException("context must be non-null");
        }

        // if no profile was passed in, look for the default profile listed in profiles.ini
        // if that doesn't exist, look for a profile called 'default'
        if (TextUtils.isEmpty(profileName) && TextUtils.isEmpty(profilePath)) {
            profileName = "default";

            INIParser parser = getProfilesINI(context);

            String profile = "";
            boolean foundDefault = false;
            for (Enumeration<INISection> e = parser.getSections().elements(); e.hasMoreElements();) {
                INISection section = e.nextElement();
                if (section.getIntProperty("Default") == 1) {
                    profile = section.getStringProperty("Name");
                    foundDefault = true;
                }
            }

            if (foundDefault)
                profileName = profile;
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
                if (mDir != null) {
                    Log.i(LOGTAG, "profile dir changed from "+mDir+" to "+dir);
                }
                mDir = dir;
            } else {
                Log.w(LOGTAG, "requested profile directory missing: "+profilePath);
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
            if (!profileMigrator.isProfileMoved()) {
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

    public boolean shouldRestoreSession() {
        Log.w(LOGTAG, "zerdatime " + SystemClock.uptimeMillis() + " - start check sessionstore.js exists");
        File dir = getDir();
        if (dir == null)
            return false;

        File sessionFile = new File(dir, "sessionstore.js");
        if (!sessionFile.exists())
            return false;

        boolean shouldRestore = (System.currentTimeMillis() - sessionFile.lastModified() < SESSION_TIMEOUT);
        Log.w(LOGTAG, "zerdatime " + SystemClock.uptimeMillis() + " - finish check sessionstore.js exists");
        return shouldRestore;
    }

    public String readSessionFile(boolean geckoReady) {
        File dir = getDir();
        if (dir == null) {
            return null;
        }

        File sessionFile = null;
        if (! geckoReady) {
            // we might have crashed, in which case sessionstore.js has tabs from last time
            sessionFile = new File(dir, "sessionstore.js");
            if (! sessionFile.exists()) {
                sessionFile = null;
            }
        }
        if (sessionFile == null) {
            // either we did not crash, so previous session was moved to sessionstore.bak on quit,
            // or sessionstore init has occurred, so previous session will always
            // be in sessionstore.bak
            sessionFile = new File(dir, "sessionstore.bak");
            // no need to check if the session file exists here; readFile will throw
            // an IOException if it does not
        }

        try {
            return readFile(sessionFile);
        } catch (IOException ioe) {
            Log.i(LOGTAG, "Unable to read session file " + sessionFile.getAbsolutePath());
            return null;
        }
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
            StringBuffer sb = new StringBuffer();
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
        StringBuffer salt = new StringBuffer(16);
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
            Log.i(LOGTAG, "WESJ - SET DEFAULT");
            profileSection.setProperty("Default", 1);
        }

        parser.addSection(profileSection);
        parser.write();

        return profileDir;
    }
}
