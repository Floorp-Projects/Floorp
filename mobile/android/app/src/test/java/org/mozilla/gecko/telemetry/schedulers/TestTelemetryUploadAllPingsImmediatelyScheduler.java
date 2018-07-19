/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.telemetry.schedulers;

import android.app.job.JobInfo;
import android.app.job.JobScheduler;
import android.app.job.JobWorkItem;
import android.content.Context;
import android.content.Intent;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mozilla.gecko.telemetry.TelemetryUploadService;
import org.mozilla.gecko.telemetry.stores.TelemetryPingStore;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import java.util.List;
import java.util.Objects;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

/**
 * Unit tests for the upload immediately scheduler.
 *
 * When we add more schedulers, we'll likely change the interface
 * (e.g. pass in current time) and these tests will be more useful.
 */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = 26)
public class TestTelemetryUploadAllPingsImmediatelyScheduler {

    private TelemetryUploadAllPingsImmediatelyScheduler testScheduler;
    private TelemetryPingStore testStore;

    @Before
    public void setUp() {
        testScheduler = new TelemetryUploadAllPingsImmediatelyScheduler();
        testStore = mock(TelemetryPingStore.class);
    }

    @Test
    public void testReadyToUpload() {
        assertTrue("Scheduler is not ready to upload",
                testScheduler.isReadyToUpload(mock(Context.class), testStore));
    }

    @Test
    public void testScheduleUpload() {
        final Context appContext = spy(getAppContext());
        final JobScheduler scheduler = spy(getJobScheduler());
        // Make sure that the internal system call to enqueue work will use our mocks
        when(appContext.getApplicationContext()).thenReturn(appContext);
        when(appContext.getSystemService(Context.JOB_SCHEDULER_SERVICE)).thenReturn(scheduler);

        testScheduler.scheduleUpload(appContext, testStore);

        final List<JobInfo> pendingJobs = scheduler.getAllPendingJobs();
        assertEquals("Telemetry upload is not not started", pendingJobs.size(), 1);

        final JobInfo uploadJob = pendingJobs.get(0);
        assertEquals("Enqueued class is not the telemetry upload service",
                TelemetryUploadService.class.getName(), uploadJob.getService().getClassName());

        final ArgumentCaptor<JobWorkItem> workCaptor = ArgumentCaptor.forClass(JobWorkItem.class);
        verify(scheduler).enqueue(any(JobInfo.class), workCaptor.capture());

        final Intent receivedIntent = workCaptor.getValue().getIntent();
        assertEquals("Work Intent has wrong action",
                TelemetryUploadService.ACTION_UPLOAD, receivedIntent.getAction());
        assertTrue("Work Intent has wrong extra",
                receivedIntent.hasExtra(TelemetryUploadService.EXTRA_STORE));
    }

    private Context getAppContext() {
        return RuntimeEnvironment.application;
    }

    private JobScheduler getJobScheduler() {
        return Objects.requireNonNull((JobScheduler)
                getAppContext().getSystemService(Context.JOB_SCHEDULER_SERVICE));
    }
}
