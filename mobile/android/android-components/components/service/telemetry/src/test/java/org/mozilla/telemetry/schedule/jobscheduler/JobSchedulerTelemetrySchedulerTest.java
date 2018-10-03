/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.schedule.jobscheduler;

import android.app.job.JobInfo;
import android.app.job.JobScheduler;
import android.content.Context;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import java.util.List;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;

@RunWith(RobolectricTestRunner.class)
public class JobSchedulerTelemetrySchedulerTest {
    @Test
    public void checkId() {
        Context context = RuntimeEnvironment.application;
        int jobId = 27;
        JobSchedulerTelemetryScheduler scheduler = new JobSchedulerTelemetryScheduler(jobId);
        JobScheduler jobScheduler = context.getSystemService(JobScheduler.class);
        assertEquals(0, jobScheduler.getAllPendingJobs().size());
        scheduler.scheduleUpload(new TelemetryConfiguration(context));
        List<JobInfo> pendingJobs = jobScheduler.getAllPendingJobs();
        assertEquals(1, pendingJobs.size());
        assertEquals(27, pendingJobs.get(0).getId());
    }
}
