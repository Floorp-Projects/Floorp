/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import java.io.File;
import java.util.Enumeration;
import java.util.Hashtable;

import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.GeckoProfileDirectories;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.GeckoThread;
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
        // "Default" is a custom profile set up by the test harness.
        mAsserter.info("Test using the test profile", GeckoProfile.CUSTOM_PROFILE);
        GeckoProfile profile = GeckoProfile.get(getActivity());
        verifyProfile(profile, GeckoProfile.CUSTOM_PROFILE,
                      GeckoThread.getActiveProfile().getDir(), true);

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
        if (name != null) {
            verifyProfile(profile, name, findDir(name), false);
            removeProfile(profile, true);
        } else {
            // Passing in null for a profile name, should get you the default
            File defaultProfile = GeckoThread.getActiveProfile().getDir();
            verifyProfile(profile, GeckoProfile.CUSTOM_PROFILE, defaultProfile, true);
        }
    }

    // Test get(Context, String, String) methods
    private void checkNameAndPathGetter(String name, boolean createBefore) {
        if (name == null) {
            checkNameAndPathGetter(name, null, createBefore);
        } else {
            checkNameAndPathGetter(name, name + "_FORCED_DIR", createBefore);
        }
    }

    // Test get(Context, String, String) methods
    private void checkNameAndPathGetter(String name, String path, boolean createBefore) {
        mAsserter.info("Test using a named profile and path", name + ", " + path);
        checkNameAndDirGetter(name, /* useFile */ false, path, /* file */ null, createBefore);
    }

    private void checkNameAndFileGetter(String name, boolean createBefore) {
        if (name == null) {
            checkNameAndFileGetter(name, null, createBefore);
        } else {
            checkNameAndFileGetter(name, new File(mozDir, name + "_FORCED_DIR"), createBefore);
        }
    }

    private void checkNameAndFileGetter(String name, File f, boolean createBefore) {
        mAsserter.info("Test using a named profile and File", name + ", " + f);
        checkNameAndDirGetter(name, /* useFile */ true, /* path */ null, f, createBefore);
    }

    private void checkNameAndDirGetter(final String name, final boolean useFile,
                                       String path, final File file,
                                       final boolean createBefore) {
        final File f;
        if (useFile) {
            f = file;
        } else if (!TextUtils.isEmpty(path)) {
            f = new File(mozDir, path);
            path = f.getAbsolutePath();
        } else {
            f = null;
        }

        if (f != null && createBefore) {
            // For some tests we create explicitly beforehand
            f.mkdir();
        }

        final File testProfileDir = GeckoThread.getActiveProfile().getDir();
        final String expectedName = name != null ? name : GeckoProfile.CUSTOM_PROFILE;

        final GeckoProfile profile;
        if (useFile) {
            profile = GeckoProfile.get(getActivity(), name, file);
        } else {
            profile = GeckoProfile.get(getActivity(), name, path);
        }

        if (name != null || f != null) {
            // GeckoProfile will create a directory and add an ini section if f is null
            // here. Therefore, when f is null, shouldHaveFound is false for the
            // verifyProfile call, and inProfileIni is true for the removeProfile call.
            verifyProfile(profile, expectedName, f, f != null);
            removeProfile(profile, f == null);
            if (name == null) {
                // A side effect of calling GeckoProfile.get with null name is it changes
                // the test profile's directory to the new directory. Restore it back.
                GeckoProfile.get(getActivity(), null, testProfileDir);
                mAsserter.is(GeckoProfile.get(getActivity()).getDir(), testProfileDir,
                             "Test profile should be restored");
            }
        } else {
            // Passing in null for a profile name and path, should get you the default
            verifyProfile(profile, expectedName, testProfileDir, true);
        }
    }

    private void checkProfileCreationDeletion() {
        // Test
        checkDefaultGetter();

        int index = 0;
        checkNamedGetter(TEST_PROFILE_NAME + (index++)); // 0
        checkNamedGetter(null);

        // name and path
        checkNameAndPathGetter(TEST_PROFILE_NAME + (index++), true);
        checkNameAndPathGetter(TEST_PROFILE_NAME + (index++), false);
        checkNameAndPathGetter(null, false);
        // null name and path
        checkNameAndPathGetter(null, TEST_PROFILE_NAME + (index++) + "_FORCED_DIR", true);
        checkNameAndPathGetter(null, TEST_PROFILE_NAME + (index++) + "_FORCED_DIR", false);
        // name and null path
        checkNameAndPathGetter(TEST_PROFILE_NAME + (index++), null, false);
        checkNameAndPathGetter(TEST_PROFILE_NAME + (index++), "", false);
        // null name and null path
        checkNameAndPathGetter(null, null, false);
        checkNameAndPathGetter(null, "", false);

        // name and path
        checkNameAndFileGetter(TEST_PROFILE_NAME + (index++), true);
        checkNameAndFileGetter(TEST_PROFILE_NAME + (index++), false);
        checkNameAndFileGetter(null, false);
        // null name and path
        checkNameAndFileGetter(null, new File(mozDir, TEST_PROFILE_NAME + (index++) + "_FORCED_DIR"), true);
        checkNameAndFileGetter(null, new File(mozDir, TEST_PROFILE_NAME + (index++) + "_FORCED_DIR"), false);
        // name and null path
        checkNameAndFileGetter(TEST_PROFILE_NAME + (index++), null, false);
        // null name and null path
        checkNameAndFileGetter(null, null, false);
    }

    // Tests of Guest profile methods
    private void checkGuestProfile() {
        final File testProfileDir = GeckoThread.getActiveProfile().getDir();

        mAsserter.info("Test getting a guest profile", "");
        GeckoProfile profile = GeckoProfile.getGuestProfile(getActivity());
        verifyProfile(profile, GeckoProfile.CUSTOM_PROFILE, getActivity().getFileStreamPath("guest"), true);
        mAsserter.ok(profile.inGuestMode(), "Profile is in guest mode", profile.getName());

        final File dir = profile.getDir();
        mAsserter.info("Test deleting a guest profile", "");
        mAsserter.ok(GeckoProfile.removeProfile(getActivity(), profile), "Cleaned up unlocked guest profile", profile.getName());
        mAsserter.ok(!dir.exists(), "Guest dir was deleted", dir.toString());

        // Restore test profile directory, which was changed in the last GeckoProfile.get call.
        GeckoProfile.get(getActivity(), null, testProfileDir);
        mAsserter.is(GeckoProfile.get(getActivity()).getDir(), testProfileDir,
                     "Test profile should be restored");
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
    private void findInProfilesIni(final String name, final File dir, final boolean shouldFind) {
        final File mozDir;
        try {
            mozDir = GeckoProfileDirectories.getMozillaDirectory(getActivity());
        } catch(Exception ex) {
            mAsserter.ok(false, "Couldn't get moz dir", ex.toString());
            return;
        }

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
    // TODO: Reconsider profile removal. Firefox would not normally remove a
    // profile. Outstanding tasks may still try to access files in the profile.
    private void removeProfile(GeckoProfile profile, boolean inProfilesIni) {
        final String name = profile.getName();
        final File dir = profile.getDir();
        findInProfilesIni(name, dir, inProfilesIni);
        mAsserter.ok(dir.exists(), "Profile dir exists before removing", dir.toString());
        mAsserter.ok(GeckoProfile.removeProfile(getActivity(), profile), "Remove was successful", name);
        mAsserter.ok(!dir.exists(), "Profile dir was deleted when it was removed", dir.toString());
        findInProfilesIni(name, dir, false);
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
