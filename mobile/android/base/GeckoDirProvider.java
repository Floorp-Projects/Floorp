/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Wes Johnston <wjohnston@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko;

import android.content.Context;
import android.os.AsyncTask;
import android.os.Build;
import android.util.Log;

import java.io.File;
import java.io.FileFilter;
import java.io.FileWriter;
import java.io.IOException;
import java.lang.Void;
import java.util.Date;
import java.util.Random;
import java.util.Map;
import java.util.HashMap;

abstract public class GeckoDirProvider
{
    private static final String LOGTAG = "GeckoDirProvider";
    private static HashMap<String, File> mProfileDirs = new HashMap<String, File>();

    /**
     * Get the default Mozilla profile directory for a given Activity instance.
     *
     * @param aContext
     *        The context for the activity. Must not be null
     * @return
     *       The profile directory.
     */
    static public File getProfileDir(final Context aContext)
            throws IllegalArgumentException, IOException {
        // XXX: TO-DO read profiles.ini to get the default profile. bug 71530
        return getProfileDir(aContext, "default");
    }

    /**
     * Get a particular profile directory for a given Activity.
     * If no profile directory currently exists, will create and return a profile directory.
     * Otherwise will return null;
     *
     * @param aContext
     *        The context for the Activity we want a profile for. Must not be null.
     * @param aProfileName
     *        The name of the profile to open. Must be a non-empty string
     * @return
     *       The profile directory.
     */
    static public File getProfileDir(final Context aContext, final String aProfileName)
            throws IllegalArgumentException, IOException {

        if (aContext == null)
            throw new IllegalArgumentException("Must provide a valid context");

        if (aProfileName == null || aProfileName.trim().equals(""))
            throw new IllegalArgumentException("Profile name: '" + aProfileName + "' is not valid");

        Log.i(LOGTAG, "Get profile dir for " + aProfileName);
        synchronized (mProfileDirs) {
            File profileDir = mProfileDirs.get(aProfileName);
            if (profileDir != null)
                return profileDir;

            // we do not want to call File.exists on startup, so we first don't
            // attempt to create the mozilla directory.
            File mozDir = GeckoDirProvider.ensureMozillaDirectory(aContext);
            profileDir = GeckoDirProvider.getProfileDir(mozDir, aProfileName);

            if (profileDir == null) {
                // Throws if cannot create.
                profileDir = GeckoDirProvider.createProfileDir(mozDir, aProfileName);
            }
            mProfileDirs.put(aProfileName, profileDir);
            return profileDir;
        }
    }

    private static File getProfileDir(final File aRoot, final String aProfileName)
            throws IllegalArgumentException {
        if (aRoot == null)
            throw new IllegalArgumentException("Invalid root directory");

        File[] profiles = aRoot.listFiles(new FileFilter() {
            public boolean accept(File pathname) {
                return pathname.getName().endsWith("." + aProfileName);
            }
        });

        if (profiles != null && profiles.length > 0)
            return profiles[0];
        return null;
    }

    private static File ensureMozillaDirectory(final Context aContext)
            throws IOException, IllegalArgumentException {
        if (aContext == null)
            throw new IllegalArgumentException("Must provide a valid context");
        File filesDir = GeckoDirProvider.getFilesDir(aContext);

        File mozDir = new File(filesDir, "mozilla");
        if (!mozDir.exists()) {
            if (!mozDir.mkdir())
                throw new IOException("Unable to create mozilla directory at " + mozDir.getPath());
        }
        return mozDir;
    }

    static final char kTable[] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                   'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
                                   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0' };

    private static File createProfileDir(final File aRootDir, final String aProfileName)
            throws IOException, IllegalArgumentException {

        if (aRootDir == null)
            throw new IllegalArgumentException("Must provide a valid root directory");

        if (aProfileName == null || aProfileName.trim().equals(""))
            throw new IllegalArgumentException("Profile name: '" + aProfileName + "' is not valid");

        // XXX: TO-DO If we already have an ini file, we should append the
        //      new profile information to it. For now we just throw an exception.
        //      see bug 715391
        final File profileIni = new File(aRootDir, "profiles.ini");
        if (profileIni.exists())
            throw new IOException("Can't create new profiles");

        String saltedName = saltProfileName(aProfileName);
        File profile = new File(aRootDir, saltedName);
        while (profile.exists()) {
            saltedName = saltProfileName(aProfileName);
            profile = new File(aRootDir, saltedName);
        }

        if (!profile.mkdir()) 
            throw new IOException("Unable to create profile at " + profile.getPath());

        Log.i(LOGTAG, "Creating new profile at " + profile.getPath());
        final String fSaltedName = saltedName;

        FileWriter outputStream = new FileWriter(profileIni, true);
        outputStream.write("[General]\n" +
                           "StartWithLastProfile=1\n" +
                           "\n" +
                           "[Profile0]\n" +
                           "Name=" + aProfileName + "\n" +
                           "IsRelative=1\n" +
                           "Path=" + fSaltedName + "\n" +
                           "Default=1\n");
        outputStream.close();

        return profile;
    }

    private static String saltProfileName(final String aName) {
        Random randomGenerator = new Random(System.nanoTime());

        StringBuilder salt = new StringBuilder();
        int i;
        for (i = 0; i < 8; ++i)
            salt.append(kTable[randomGenerator.nextInt(kTable.length)]);

        salt.append(".");
        return salt.append(aName).toString();
    }

    public static File getFilesDir(final Context aContext) {
        if (aContext == null)
            throw new IllegalArgumentException("Must provide a valid context");

        if (Build.VERSION.SDK_INT < 8 ||
            aContext.getPackageResourcePath().startsWith("/data") ||
            aContext.getPackageResourcePath().startsWith("/system")) {
            return aContext.getFilesDir();
        }
        return aContext.getExternalFilesDir(null);
    }
}
