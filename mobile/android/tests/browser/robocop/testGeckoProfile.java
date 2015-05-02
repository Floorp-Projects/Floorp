/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import java.io.File;
import java.util.Enumeration;
import java.util.Hashtable;

import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.GeckoProfileDirectories;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.util.INIParser;
import org.mozilla.gecko.util.INISection;

import android.content.Context;
import android.text.TextUtils;

/**
 * This patch tests GeckoProfile. It has unit tests for basic getting and removing of profiles, as well as
 * some guest mode tests. It does not test locking and unlocking profiles yet. It does not test the file management in GeckoProfile.
 */

public class testGeckoProfile extends PixelTest {
    private final String TEST_PROFILE_NAME = "testProfile";
    private File mozDir;
    public void testGeckoProfile() {
        blockForGeckoReady();

        try {
            mozDir = GeckoProfileDirectories.getMozillaDirectory(getActivity());
        } catch(Exception ex) {
            // If we can't get the moz dir, something is wrong. Just fail quickly.
            mAsserter.ok(false, "Couldn't get moz dir", ex.toString());
            return;
        }

        checkProfileCreationDeletion();
        checkGuestProfile();
    }

    // This getter just passes an activity. Passing null should throw.
    private void checkDefaultGetter() {
        mAsserter.info("Test using the default profile", GeckoProfile.DEFAULT_PROFILE);
        GeckoProfile profile = GeckoProfile.get(getActivity());
        // This profile has been forced into something strange by the test harness, but its name is still right...
        verifyProfile(profile, GeckoProfile.DEFAULT_PROFILE, ((GeckoApp) getActivity()).getProfile().getDir(), true);

        try {
            profile = GeckoProfile.get(null);
            mAsserter.ok(false, "Passing a null context should throw", profile.toString());
        } catch(Exception ex) {
            mAsserter.ok(true, "Passing a null context should throw", ex.toString());
        }
    }

    // Test get(Context, String) methods
    private void checkNamedGetter(String name) {
        mAsserter.info("Test using a named profile", name);
        GeckoProfile profile = GeckoProfile.get(getActivity(), name);
        if (!TextUtils.isEmpty(name)) {
            verifyProfile(profile, name, findDir(name), false);
            removeProfile(profile, true);
        } else {
            // Passing in null for a profile name, should get you the default
            File defaultProfile = ((GeckoApp) getActivity()).getProfile().getDir();
            verifyProfile(profile, GeckoProfile.DEFAULT_PROFILE, defaultProfile, true);
        }
    }

    // Test get(Context, String, String) methods
    private void checkNameAndPathGetter(String name, boolean createBefore) {
        if (TextUtils.isEmpty(name)) {
            checkNameAndPathGetter(name, null, createBefore);
        } else {
            checkNameAndPathGetter(name, name + "_FORCED_DIR", createBefore);
        }
    }

    // Test get(Context, String, String) methods
    private void checkNameAndPathGetter(String name, String path, boolean createBefore) {
        mAsserter.info("Test using a named profile and path", name + ", " + path);

        File f = null;
        if (!TextUtils.isEmpty(path)) {
            f = new File(mozDir, path);
            // GeckoProfile will ignore dirs passed in if they don't exist. For some tests we create explicitly beforehand
            if (createBefore) {
                f.mkdir();
            }
            path = f.getAbsolutePath();
        }

        try {
            GeckoProfile profile = GeckoProfile.get(getActivity(), name, path);
            if (!TextUtils.isEmpty(name)) {
                verifyProfile(profile, name, f, createBefore);
                removeProfile(profile, !createBefore);
            } else {
                mAsserter.ok(TextUtils.isEmpty(path), "Passing a null name and non-null path should throw", name + ", " + path);
                // Passing in null for a profile name and path, should get you the default
                File defaultProfile = ((GeckoApp) getActivity()).getProfile().getDir();
                verifyProfile(profile, GeckoProfile.DEFAULT_PROFILE, defaultProfile, true);
            }
        } catch(Exception ex) {
            mAsserter.ok(TextUtils.isEmpty(name) && !TextUtils.isEmpty(path), "Passing a null name and non null path should throw", name + ", " + path);
        }
    }

    private void checkNameAndFileGetter(String name, boolean createBefore) {
        if (TextUtils.isEmpty(name)) {
            checkNameAndFileGetter(name, null, createBefore);
        } else {
            checkNameAndFileGetter(name, new File(mozDir, name + "_FORCED_DIR"), createBefore);
        }
    }

    private void checkNameAndFileGetter(String name, File f, boolean createBefore) {
        mAsserter.info("Test using a named profile and path", name + ", " + f);
        if (f != null && createBefore) {
            f.mkdir();
        }

        try {
            GeckoProfile profile = GeckoProfile.get(getActivity(), name, f);
            if (!TextUtils.isEmpty(name)) {
                verifyProfile(profile, name, f, createBefore);
                removeProfile(profile, !createBefore);
            } else {
                mAsserter.ok(f == null, "Passing a null name and non-null file should throw", name + ", " + f);
                // Passing in null for a profile name and path, should get you the default
                File defaultProfile = ((GeckoApp) getActivity()).getProfile().getDir();
                verifyProfile(profile, GeckoProfile.DEFAULT_PROFILE, defaultProfile, true);
            }
        } catch(Exception ex) {
            mAsserter.ok(TextUtils.isEmpty(name) && f != null, "Passing a null name and non null file should throw", name + ", " + f);
        }
    }

    private void checkProfileCreationDeletion() {
        // Test
        checkDefaultGetter();

        int index = 0;
        checkNamedGetter(TEST_PROFILE_NAME + (index++)); // 0
        checkNamedGetter("");
        checkNamedGetter(null);

        // name and path
        checkNameAndPathGetter(TEST_PROFILE_NAME + (index++), true); // 1
        checkNameAndPathGetter(TEST_PROFILE_NAME + (index++), false); // 2
        // null name and path
        checkNameAndPathGetter(null, TEST_PROFILE_NAME + (index++) + "_FORCED_DIR", true); // 3
        checkNameAndPathGetter(null, TEST_PROFILE_NAME + (index++) + "_FORCED_DIR", false); // 4
        checkNameAndPathGetter("", TEST_PROFILE_NAME + (index++) + "_FORCED_DIR", true); // 5
        checkNameAndPathGetter("", TEST_PROFILE_NAME + (index++) + "_FORCED_DIR", false); // 6
        // name and null path
        checkNameAndPathGetter(TEST_PROFILE_NAME + (index++), null, false); // 7
        checkNameAndPathGetter(TEST_PROFILE_NAME + (index++), "", false); // 8
        // null name and null path
        checkNameAndPathGetter(null, null, false);
        checkNameAndPathGetter("", null, false);
        checkNameAndPathGetter(null, "", false);
        checkNameAndPathGetter("", "", false);

        // name and path
        checkNameAndFileGetter(TEST_PROFILE_NAME + (index++), true); // 9
        checkNameAndFileGetter(TEST_PROFILE_NAME + (index++), false); // 10
        // null name and path
        checkNameAndFileGetter(null, new File(mozDir, TEST_PROFILE_NAME + (index++) + "_FORCED_DIR"), true); // 11
        checkNameAndFileGetter(null, new File(mozDir, TEST_PROFILE_NAME + (index++) + "_FORCED_DIR"), false); // 12
        checkNameAndFileGetter("", new File(mozDir, TEST_PROFILE_NAME + (index++) + "_FORCED_DIR"), true); // 13
        checkNameAndFileGetter("", new File(mozDir, TEST_PROFILE_NAME + (index++) + "_FORCED_DIR"), false); // 14
        // name and null path
        checkNameAndFileGetter(TEST_PROFILE_NAME + (index++), null, false); // 16
        // null name and null path
        checkNameAndFileGetter(null, null, false);
    }

    // Tests of Guest profile methods
    private void checkGuestProfile() {
        mAsserter.info("Test getting a guest profile", "");
        GeckoProfile profile = GeckoProfile.createGuestProfile(getActivity());
        verifyProfile(profile, GeckoProfile.GUEST_PROFILE, getActivity().getFileStreamPath("guest"), true);
        File dir = profile.getDir();

        mAsserter.ok(profile.inGuestMode(), "Profile is in guest mode", profile.getName());

        mAsserter.info("Test deleting a guest profile", "");
        mAsserter.ok(!GeckoProfile.maybeCleanupGuestProfile(getActivity()), "Can't clean up locked guest profile", profile.getName());
        GeckoProfile.leaveGuestSession(getActivity());
        mAsserter.ok(GeckoProfile.maybeCleanupGuestProfile(getActivity()), "Cleaned up unlocked guest profile", profile.getName());
        mAsserter.ok(!dir.exists(), "Guest dir was deleted", dir.toString());
    }

    // Runs generic tests on a profile to make sure it looks correct
    private void verifyProfile(GeckoProfile profile, String name, File requestedDir, boolean shouldHaveFound) {
        mAsserter.is(profile.getName(), name, "Profile name is correct");

        File dir = null;
        if (!shouldHaveFound) {
            mAsserter.is(findDir(name), null, "Dir with name doesn't exist yet");

            dir = profile.getDir();
            mAsserter.isnot(requestedDir, dir, "Profile should not have used expectedDir");

            // The used dir should be based on the name passed in.
            requestedDir = findDir(name);
        } else {
            dir = profile.getDir();
        }

        mAsserter.is(dir, requestedDir, "Profile dir is correct");
        mAsserter.ok(dir.exists(), "Profile dir exists after getting it", dir.toString());
    }

    // Tries to find a profile in profiles.ini. Makes sure its name and path match what is expected
    private void findInProfilesIni(GeckoProfile profile, boolean shouldFind) {
        final File mozDir;
        try {
            mozDir = GeckoProfileDirectories.getMozillaDirectory(getActivity());
        } catch(Exception ex) {
            mAsserter.ok(false, "Couldn't get moz dir", ex.toString());
            return;
        }

        final String name = profile.getName();
        final File dir = profile.getDir();

        final INIParser parser = GeckoProfileDirectories.getProfilesINI(mozDir);
        final Hashtable<String, INISection> sections = parser.getSections();

        boolean found = false;
        for (Enumeration<INISection> e = sections.elements(); e.hasMoreElements();) {
            final INISection section = e.nextElement();
            String iniName = section.getStringProperty("Name");
            if (iniName == null || !iniName.equals(name)) {
                continue;
            }

            found = true;

            String iniPath = section.getStringProperty("Path");
            mAsserter.is(name, iniName, "Section with name found");
            mAsserter.is(dir.getName(), iniPath, "Section has correct path");
        }

        mAsserter.is(found, shouldFind, "Found profile where expected");
    }

    // Tries to remove a profile from Gecko profile. Verifies that it's removed from profiles.ini and its directory is deleted.
    private void removeProfile(GeckoProfile profile, boolean inProfilesIni) {
        findInProfilesIni(profile, inProfilesIni);
        File dir = profile.getDir();
        mAsserter.ok(dir.exists(), "Profile dir exists before removing", dir.toString());
        mAsserter.is(inProfilesIni, GeckoProfile.removeProfile(getActivity(), profile.getName()), "Remove was successful");
        mAsserter.ok(!dir.exists(), "Profile dir was deleted when it was removed", dir.toString());
        findInProfilesIni(profile, false);
    }

    // Looks for a dir whose name ends with the passed-in string.
    private File findDir(String name) {
        final File root;
        try {
            root = GeckoProfileDirectories.getMozillaDirectory(getActivity());
        } catch(Exception ex) {
            return null;
        }

        File[] dirs = root.listFiles();
        for (File dir : dirs) {
            if (dir.getName().endsWith(name)) {
                return dir;
            }
        }

        return null;
    }

    @Override
    public void tearDown() throws Exception {
        // Clear SharedPreferences.
        final Context context = getInstrumentation().getContext();
        GeckoSharedPrefs.forProfile(context).edit().clear().apply();

        super.tearDown();
    }
}
