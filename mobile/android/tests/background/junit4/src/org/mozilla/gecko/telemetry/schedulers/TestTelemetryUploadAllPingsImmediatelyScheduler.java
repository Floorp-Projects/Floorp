/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.telemetry.schedulers;

import android.content.Context;
import android.content.Intent;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.telemetry.TelemetryUploadService;
import org.mozilla.gecko.telemetry.stores.TelemetryPingStore;

import static junit.framework.Assert.*;
import static org.mockito.Mockito.*;

/**
 * Unit tests for the upload immediately scheduler.
 *
 * When we add more schedulers, we'll likely change the interface
 * (e.g. pass in current time) and these tests will be more useful.
 */
@RunWith(TestRunner.class)
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
        assertTrue("Scheduler is always ready to upload", testScheduler.isReadyToUpload(testStore));
    }

    @Test
    public void testScheduleUpload() {
        final Context context = mock(Context.class);

        testScheduler.scheduleUpload(context, testStore);

        final ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(context).startService(intentCaptor.capture());
        final Intent actualIntent = intentCaptor.getValue();
        assertEquals("Intent action is upload", TelemetryUploadService.ACTION_UPLOAD, actualIntent.getAction());
        assertTrue("Intent contains store", actualIntent.hasExtra(TelemetryUploadService.EXTRA_STORE));
        assertEquals("Intent class target is upload service",
                TelemetryUploadService.class.getName(), actualIntent.getComponent().getClassName());
    }
}
