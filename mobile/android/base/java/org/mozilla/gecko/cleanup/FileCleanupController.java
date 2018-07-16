/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.cleanup;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.support.annotation.VisibleForTesting;

import java.io.File;
import java.util.ArrayList;
import java.util.concurrent.TimeUnit;

/**
 * Encapsulates the code to run the {@link FileCleanupService}. Call
 * {@link #startIfReady(Context, SharedPreferences, String)} to start the clean-up.
 *
 * Note: for simplicity, the current implementation does not cache which
 * files have been cleaned up and will attempt to delete the same files
 * each time it is run. If the file deletion list grows large, consider
 * keeping a cache.
 */
public class FileCleanupController {

    private static final long MILLIS_BETWEEN_CLEANUPS = TimeUnit.DAYS.toMillis(7);
    @VisibleForTesting static final String PREF_LAST_CLEANUP_MILLIS = "cleanup.lastFileCleanupMillis";

    // These will be prepended with the path of the profile we're cleaning up.
    private static final String[] PROFILE_FILES_TO_CLEANUP = new String[] {
            "health.db",
            "health.db-journal",
            "health.db-shm",
            "health.db-wal",
    };

    /**
     * Starts the clean-up if it's time to clean-up, otherwise returns. For simplicity,
     * it does not schedule the cleanup for some point in the future - this method will
     * have to be called again (i.e. polled) in order to run the clean-up service.
     *
     * @param context Context of the calling {@link android.app.Activity}
     * @param sharedPrefs The {@link SharedPreferences} instance to store the controller state to
     * @param profilePath The path to the profile the service should clean-up files from
     */
    public static void startIfReady(final Context context, final SharedPreferences sharedPrefs, final String profilePath) {
        if (!isCleanupReady(sharedPrefs)) {
            return;
        }

        recordCleanupScheduled(sharedPrefs);

        final Intent fileCleanupIntent =
                FileCleanupService.getFileCleanupIntent(context, getFilesToCleanup(profilePath + "/"));
        FileCleanupService.enqueueWork(context, fileCleanupIntent);
    }

    private static boolean isCleanupReady(final SharedPreferences sharedPrefs) {
        final long lastCleanupMillis = sharedPrefs.getLong(PREF_LAST_CLEANUP_MILLIS, -1);
        return lastCleanupMillis + MILLIS_BETWEEN_CLEANUPS < System.currentTimeMillis();
    }

    private static void recordCleanupScheduled(final SharedPreferences sharedPrefs) {
        final SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.putLong(PREF_LAST_CLEANUP_MILLIS, System.currentTimeMillis()).apply();
    }

    @VisibleForTesting
    static ArrayList<String> getFilesToCleanup(final String profilePath) {
        final ArrayList<String> out = new ArrayList<>(PROFILE_FILES_TO_CLEANUP.length);
        for (final String path : PROFILE_FILES_TO_CLEANUP) {
            // Append a file separator, just in-case the caller didn't include one.
            out.add(profilePath + File.separator + path);
        }
        return out;
    }
}
