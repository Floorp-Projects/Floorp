/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry;

import org.junit.Test;

import static org.junit.Assert.*;
import static org.mockito.Mockito.mock;

public class TelemetryHolderTest {
    @Test(expected = IllegalStateException.class)
    public void callingGetterWithoutSettingsThrowsException() {
        TelemetryHolder.set(null);
        TelemetryHolder.get();
    }

    @Test
    public void testGetterReturnsSameInstance() {
        Telemetry telemetry = mock(Telemetry.class);

        TelemetryHolder.set(telemetry);

        assertEquals(telemetry, TelemetryHolder.get());
    }
}