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
import java.io.FileInputStream;
import java.io.FileOutputStream;
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

    private GeckoProfile(Context context, String profileName) {
        mContext = context;
        mName = profileName;
    }

    public synchronized File getDir() {
        if (mDir != null) {
            return mDir;
        }

        try {
            File mozillaDir = ensureMozillaDirectory();
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

    public File getFilesDir() {
        if (isOnInternalStorage()) {
            return mContext.getFilesDir();
        } else {
            return mContext.getExternalFilesDir(null);
        }
    }

    private boolean isOnInternalStorage() {
        // prior to version 8, apps were always on internal storage
        if (Build.VERSION.SDK_INT < 8) {
            return true;
        }
        // if there is no external storage dir, then we're definitely on internal storage
        File externalDir = mContext.getExternalFilesDir(null);
        if (externalDir == null) {
            return true;
        }
        // otherwise, check app install location to see if it is on internal storage
        String resourcePath = mContext.getPackageResourcePath();
        if (resourcePath.startsWith("/data") || resourcePath.startsWith("/system")) {
            return true;
        }

        // otherwise we're most likely on external storage
        return false;
    }

    public void moveProfilesToAppInstallLocation() {
        // check normal install directory
        moveProfilesFrom(new File("/data/data/" + mContext.getPackageName()));

        if (isOnInternalStorage()) {
            if (Build.VERSION.SDK_INT >= 8) {
                // if we're currently on internal storage, but we're on API >= 8, so it's possible that
                // we were previously on external storage, check there for profiles to pull in
                moveProfilesFrom(mContext.getExternalFilesDir(null));
            }
        } else {
            // we're currently on external storage, but could have been on internal storage previously,
            // so pull in those profiles
            moveProfilesFrom(mContext.getFilesDir());
        }
    }

    private void moveProfilesFrom(File oldFilesDir) {
        if (oldFilesDir == null) {
            return;
        }
        File oldMozDir = new File(oldFilesDir, "mozilla");
        if (! (oldMozDir.exists() && oldMozDir.isDirectory())) {
            return;
        }

        // if we get here, we know that oldMozDir exists

        File currentMozDir;
        try {
            currentMozDir = ensureMozillaDirectory();
            if (currentMozDir.equals(oldMozDir)) {
                return;
            }
        } catch (IOException ioe) {
            Log.e(LOGTAG, "Unable to create a profile directory!", ioe);
            return;
        }

        Log.d(LOGTAG, "Moving old profile directories from " + oldMozDir.getAbsolutePath());

        // if we get here, we know that oldMozDir != currentMozDir, so we have some stuff to move
        moveDirContents(oldMozDir, currentMozDir);
    }

    private void moveDirContents(File src, File dst) {
        File[] files = src.listFiles();
        if (files == null) {
            src.delete();
            return;
        }
        for (File f : files) {
            File target = new File(dst, f.getName());
            try {
                if (f.renameTo(target)) {
                    continue;
                }
            } catch (SecurityException se) {
                Log.e(LOGTAG, "Unable to rename file to " + target.getAbsolutePath() + " while moving profiles", se);
            }
            // rename failed, try moving manually
            if (f.isDirectory()) {
                if (target.mkdirs()) {
                    moveDirContents(f, target);
                } else {
                    Log.e(LOGTAG, "Unable to create folder " + target.getAbsolutePath() + " while moving profiles");
                }
            } else {
                if (! moveFile(f, target)) {
                    Log.e(LOGTAG, "Unable to move file " + target.getAbsolutePath() + " while moving profiles");
                }
            }
        }
        src.delete();
    }

    private boolean moveFile(File src, File dst) {
        boolean success = false;
        long lastModified = src.lastModified();
        try {
            FileInputStream fis = new FileInputStream(src);
            try {
                FileOutputStream fos = new FileOutputStream(dst);
                try {
                    FileChannel inChannel = fis.getChannel();
                    long size = inChannel.size();
                    if (size == inChannel.transferTo(0, size, fos.getChannel())) {
                        success = true;
                    }
                } finally {
                    fos.close();
                }
            } finally {
                fis.close();
            }
        } catch (IOException ioe) {
            Log.e(LOGTAG, "Exception while attempting to move file to " + dst.getAbsolutePath(), ioe);
        }

        if (success) {
            dst.setLastModified(lastModified);
            src.delete();
        } else {
            dst.delete();
        }
        return success;
    }

    private synchronized File ensureMozillaDirectory() throws IOException {
        if (mMozDir != null) {
            return mMozDir;
        }

        File filesDir = getFilesDir();
        File mozDir = new File(filesDir, "mozilla");
        if (! mozDir.exists()) {
            if (! mozDir.mkdirs()) {
                throw new IOException("Unable to create mozilla directory at " + mozDir.getAbsolutePath());
            }
        }
        mMozDir = mozDir;
        return mMozDir;
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
