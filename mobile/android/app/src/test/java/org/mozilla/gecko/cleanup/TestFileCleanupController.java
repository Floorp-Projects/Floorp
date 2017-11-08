/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.cleanup;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.robolectric.RuntimeEnvironment;

import java.util.ArrayList;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.atMost;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

/**
 * Tests functionality of the {@link FileCleanupController}.
 */
@RunWith(TestRunner.class)
public class TestFileCleanupController {

    @Test
    public void testStartIfReadyEmptySharedPrefsRunsCleanup() {
        final Context context = mock(Context.class);
        FileCleanupController.startIfReady(context, getSharedPreferences(), "");
        verify(context).startService(any(Intent.class));
    }

    @Test
    public void testStartIfReadyLastRunNowDoesNotRun() {
        final SharedPreferences sharedPrefs = getSharedPreferences();
        sharedPrefs.edit()
                .putLong(FileCleanupController.PREF_LAST_CLEANUP_MILLIS, System.currentTimeMillis())
                .commit(); // synchronous to finish before test runs.

        final Context context = mock(Context.class);
        FileCleanupController.startIfReady(context, sharedPrefs, "");

        verify(context, never()).startService((any(Intent.class)));
    }

    /**
     * Depends on {@link #testStartIfReadyEmptySharedPrefsRunsCleanup()} success –
     * i.e. we expect the cleanup to run with empty prefs.
     */
    @Test
    public void testStartIfReadyDoesNotRunTwiceInSuccession() {
        final Context context = mock(Context.class);
        final SharedPreferences sharedPrefs = getSharedPreferences();

        FileCleanupController.startIfReady(context, sharedPrefs, "");
        verify(context).startService(any(Intent.class));

        // Note: the Controller relies on SharedPrefs.apply, but
        // robolectric made this a synchronous call. Yay!
        FileCleanupController.startIfReady(context, sharedPrefs, "");
        verify(context, atMost(1)).startService(any(Intent.class));
    }

    @Test
    public void testGetFilesToCleanupContainsProfilePath() {
        final String profilePath = "/a/profile/path";
        final ArrayList<String> fileList = FileCleanupController.getFilesToCleanup(profilePath);
        assertNotNull("Returned file list is non-null", fileList);

        boolean atLeastOneStartsWithProfilePath = false;
        final String pathToCheck = profilePath + "/"; // Ensure the calling code adds a slash to divide the path.
        for (final String path : fileList) {
            if (path.startsWith(pathToCheck)) {
                // It'd be great if we could assert these individually so
                // we could display the Strings in console output.
                atLeastOneStartsWithProfilePath = true;
            }
        }
        assertTrue("At least one returned String starts with a profile path", atLeastOneStartsWithProfilePath);
    }

    private SharedPreferences getSharedPreferences() {
        return RuntimeEnvironment.application.getSharedPreferences("TestFileCleanupController", 0);
    }
}
