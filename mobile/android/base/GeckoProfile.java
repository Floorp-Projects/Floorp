/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko;

import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.nio.channels.FileChannel;
import java.util.HashMap;
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

    public static GeckoProfile get(Context context) {
        return get(context, null);
    }

    public static GeckoProfile get(Context context, String profileName) {
        if (context == null) {
            throw new IllegalArgumentException("context must be non-null");
        }
        if (TextUtils.isEmpty(profileName)) {
            // XXX: TO-DO read profiles.ini to get the default profile. bug 715307
            profileName = "default";
        }

        synchronized (sProfileCache) {
            GeckoProfile profile = sProfileCache.get(profileName);
            if (profile == null) {
                profile = new GeckoProfile(context, profileName);
                sProfileCache.put(profileName, profile);
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

    private GeckoProfile(Context context, String profileName) {
        mContext = context;
        mName = profileName;
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

            File mozillaDir = ensureMozillaDirectory(mContext);
            mDir = findProfileDir(mozillaDir);
            if (mDir == null) {
                mDir = createProfileDir(mozillaDir);
            } else {
                Log.d(LOGTAG, "Found profile dir: " + mDir.getAbsolutePath());
            }
        } catch (IOException ioe) {
            Log.e(LOGTAG, "Error getting profile dir", ioe);
        }
        return mDir;
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

    private File findProfileDir(File mozillaDir) {
        String suffix = '.' + mName;
        File[] candidates = mozillaDir.listFiles();
        if (candidates == null) {
            return null;
        }
        for (File f : candidates) {
            if (f.isDirectory() && f.getName().endsWith(suffix)) {
                return f;
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
        // XXX: TO-DO If we already have an ini file, we should append the
        //      new profile information to it. For now we just throw an exception.
        //      see bug 715391
        File profileIniFile = new File(mozillaDir, "profiles.ini");
        if (profileIniFile.exists()) {
            throw new IOException("Can't create new profiles");
        }

        String saltedName = saltProfileName(mName);
        File profileDir = new File(mozillaDir, saltedName);
        while (profileDir.exists()) {
            saltedName = saltProfileName(mName);
            profileDir = new File(mozillaDir, saltedName);
        }

        if (! profileDir.mkdirs()) {
            throw new IOException("Unable to create profile at " + profileDir.getAbsolutePath());
        }
        Log.d(LOGTAG, "Created new profile dir at " + profileDir.getAbsolutePath());

        FileWriter out = new FileWriter(profileIniFile, true);
        try {
            out.write("[General]\n" +
                      "StartWithLastProfile=1\n" +
                      "\n" +
                      "[Profile0]\n" +
                      "Name=" + mName + "\n" +
                      "IsRelative=1\n" +
                      "Path=" + saltedName + "\n" +
                      "Default=1\n");
        } finally {
            out.close();
        }

        return profileDir;
    }
}
