/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.net;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.shadows.ShadowLog;

import java.io.PrintStream;

import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

@RunWith(RobolectricTestRunner.class)
public class DebugLogClientTest {
    @Test
    public void testDebugLogClientWritesSomethingToLog() {
        final PrintStream stream = mock(PrintStream.class);
        ShadowLog.stream = stream;

        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);

        final DebugLogClient client = new DebugLogClient("TEST");

        // Nothing has written to the log so far
        verify(stream, never()).println(anyString());

        client.uploadPing(configuration, "path", "{'hello': 'world'}");

        // Our client has logged two lines, a separator and the JSON for the ping
        verify(stream, times(2)).println(anyString());
    }
}