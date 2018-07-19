/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.cleanup;

import android.app.job.JobScheduler;
import android.content.Context;
import android.content.SharedPreferences;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import java.util.ArrayList;
import java.util.Objects;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

/**
 * Tests functionality of the {@link FileCleanupController}.
 */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = 26)
public class TestFileCleanupController {

    @Test
    public void testStartIfReadyEmptySharedPrefsRunsCleanup() {
        final Context appContext = getAppContext();
        final JobScheduler scheduler = getJobScheduler();

        FileCleanupController.startIfReady(appContext, getSharedPreferences(), "");
        assertTrue("File cleanup is not started", scheduler.getAllPendingJobs().size() == 1);
    }

    @Test
    public void testStartIfReadyLastRunNowDoesNotRun() {
        final Context appContext = getAppContext();
        final JobScheduler scheduler = getJobScheduler();
        final SharedPreferences sharedPrefs = getSharedPreferences();
        sharedPrefs.edit()
                .putLong(FileCleanupController.PREF_LAST_CLEANUP_MILLIS, System.currentTimeMillis())
                .commit(); // synchronous to finish before test runs.

        FileCleanupController.startIfReady(appContext, sharedPrefs, "");

        assertTrue("There is a job scheduled", scheduler.getAllPendingJobs().size() == 0);
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
        return getAppContext().getSharedPreferences("TestFileCleanupController", 0);
    }

    private Context getAppContext() {
        return RuntimeEnvironment.application;
    }

    private JobScheduler getJobScheduler() {
        return Objects.requireNonNull((JobScheduler)
                getAppContext().getSystemService(Context.JOB_SCHEDULER_SERVICE));
    }
}
