/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.schedule.jobscheduler;

import android.app.job.JobInfo;
import android.app.job.JobScheduler;
import android.content.ComponentName;
import android.content.Context;

import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.mozilla.telemetry.schedule.TelemetryScheduler;

/**
 * TelemetryScheduler implementation that uses Android's JobScheduler API to schedule ping uploads.
 */
public class JobSchedulerTelemetryScheduler implements TelemetryScheduler {
    private static final int DEFAULT_JOB_ID = 42;

    private final int jobId;

    public JobSchedulerTelemetryScheduler() {
        this(DEFAULT_JOB_ID);
    }

    public JobSchedulerTelemetryScheduler(int jobId) {
        this.jobId = jobId;
    }

    @Override
    public void scheduleUpload(TelemetryConfiguration configuration) {
        final ComponentName jobService = new ComponentName(configuration.getContext(), TelemetryJobService.class);

        final JobInfo jobInfo = new JobInfo.Builder(jobId, jobService)
                .setRequiredNetworkType(JobInfo.NETWORK_TYPE_ANY)
                .setPersisted(true)
                .setBackoffCriteria(configuration.getInitialBackoffForUpload(), JobInfo.BACKOFF_POLICY_EXPONENTIAL)
                .build();

        final JobScheduler scheduler = (JobScheduler) configuration.getContext()
                .getSystemService(Context.JOB_SCHEDULER_SERVICE);

        scheduler.schedule(jobInfo);
    }
}
