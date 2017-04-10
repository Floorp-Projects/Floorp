/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.util;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import static org.junit.Assert.*;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

@RunWith(RobolectricTestRunner.class)
public class ThreadUtilsTest {
    @Test
    public void runOnThreadAndReturn() throws Exception {
        final Runnable runnable = mock(Runnable.class);

        final Thread thread = ThreadUtils.runOnThreadAndReturn("test-name", runnable);
        assertNotNull(thread);

        assertEquals("test-name", thread.getName());

        thread.join();

        verify(runnable).run();
    }
}