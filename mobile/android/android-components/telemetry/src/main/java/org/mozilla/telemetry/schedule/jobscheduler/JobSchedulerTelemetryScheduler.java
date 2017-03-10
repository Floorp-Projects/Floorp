/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.schedule.jobscheduler;

import android.app.job.JobInfo;
import android.app.job.JobScheduler;
import android.content.ComponentName;
import android.content.Context;
import android.os.PersistableBundle;

import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.mozilla.telemetry.schedule.TelemetryScheduler;

/**
 * TelemetryScheduler implementation that uses Android's JobScheduler API to schedule ping uploads.
 */
public class JobSchedulerTelemetryScheduler implements TelemetryScheduler {
    public static final int JOB_ID = 42;

    @Override
    public void scheduleUpload(TelemetryConfiguration configuration, String pingType) {
        final ComponentName jobService = new ComponentName(configuration.getContext(), TelemetryJobService.class);

        final PersistableBundle extras = new PersistableBundle();
        extras.putString(TelemetryJobService.EXTRA_PING_TYPE, pingType);

        final JobInfo jobInfo = new JobInfo.Builder(JOB_ID, jobService)
                .setRequiredNetworkType(JobInfo.NETWORK_TYPE_ANY)
                .setPersisted(true)
                .setBackoffCriteria(configuration.getInitialBackoffForUpload(), JobInfo.BACKOFF_POLICY_EXPONENTIAL)
                .setExtras(extras)
                .build();

        final JobScheduler scheduler = (JobScheduler) configuration.getContext()
                .getSystemService(Context.JOB_SCHEDULER_SERVICE);

        scheduler.schedule(jobInfo);
    }
}
