/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

/**
 * <p>
 * Repository for all the IDs used to differentiate between different Android Jobs.<br>
 * They should be unique across both the app’s own code and the code from any library that the app uses.<br>
 * To try and mitigate this situations we will use IDs starting from 1000, same as used on Android examples.
 * </p>
 * Because as per <a href="https://bugzilla.mozilla.org/show_bug.cgi?id=1437871">Bug 1437871</a>
 * both Firefox Release and Firefox Beta <em>use the same shared UID</em> we will use an offset to
 * differentiate between the Job Ids of the two apps to avoid any potential problems.
 *
 * @see <a href="https://developer.android.com/reference/android/app/job/JobInfo.Builder#JobInfo.Builder(int, android.content.ComponentName)">
 *     JobId importance</a>
 */
public class JobIdsConstants {
    // Offset to differentiate the ids for Release/Beta versions of the app
    private static final int BETA_OFFSET = 1000;

    private static final int JOB_ID_DLC_STUDY = 1000;
    private static final int JOB_ID_DLC_DOWNLOAD = 1001;
    private static final int JOB_ID_DLC_SYNCHRONIZE = 1002;
    private static final int JOB_ID_DLC_CLEANUP = 1003;

    private static final int JOB_ID_TAB_RECEIVED = 1004;

    private static final int JOB_ID_PROFILE_FETCH = 1005;
    private static final int JOB_ID_PROFILE_DELETE = 1006;

    private static final int JOB_ID_TELEMETRY_UPLOAD = 1007;

    private static final int JOB_ID_FILE_CLEANUP = 1008;

    private static final int JOB_ID_UPDATES_REGISTER = 1009;
    private static final int JOB_ID_UPDATES_CHECK_FOR = 1010;
    private static final int JOB_ID_UPDATES_DOWNLOAD = 1011;
    private static final int JOB_ID_UPDATES_APPLY = 1012;

    public static int getIdForDlcStudyJob() {
        return getIdWithOffset(JOB_ID_DLC_STUDY);
    }

    public static int getIdForDlcDownloadJob() {
        return getIdWithOffset(JOB_ID_DLC_DOWNLOAD);
    }

    public static int getIdForDlcSynchronizeJob() {
        return getIdWithOffset(JOB_ID_DLC_SYNCHRONIZE);
    }

    public static int getIdForDlcCleanupJob() {
        return getIdWithOffset(JOB_ID_DLC_CLEANUP);
    }

    public static int getIdForTabReceivedJob() {
        return getIdWithOffset(JOB_ID_TAB_RECEIVED);
    }

    public static int getIdForProfileFetchJob() {
        return getIdWithOffset(JOB_ID_PROFILE_FETCH);
    }

    public static int getIdForProfileDeleteJob() {
        return getIdWithOffset(JOB_ID_PROFILE_DELETE);
    }

    public static int getIdForTelemetryUploadJob() {
        return getIdWithOffset(JOB_ID_TELEMETRY_UPLOAD);
    }

    public static int getIdForFileCleanupJob() {
        return getIdWithOffset(JOB_ID_FILE_CLEANUP);
    }

    public static int getIdForUpdatesRegisterJob() {
        return getIdWithOffset(JOB_ID_UPDATES_REGISTER);
    }

    public static int getIdForUpdatesCheckJob() {
        return getIdWithOffset(JOB_ID_UPDATES_CHECK_FOR);
    }

    public static int getIdForUpdatesDownloadJob() {
        return getIdWithOffset(JOB_ID_UPDATES_DOWNLOAD);
    }

    public static int getIdForUpdatesApplyJob() {
        return getIdWithOffset(JOB_ID_UPDATES_APPLY);
    }

    private static boolean isReleaseBuild() {
        return AppConstants.RELEASE_OR_BETA;
    }

    private static int getIdWithOffset(final int jobIdUsedInRelease) {
        return isReleaseBuild() ? jobIdUsedInRelease : jobIdUsedInRelease + BETA_OFFSET;
    }
}
