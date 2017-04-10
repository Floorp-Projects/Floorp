/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.util;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import java.io.Closeable;
import java.io.IOException;

import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

@RunWith(RobolectricTestRunner.class)
public class IOUtilsTest {
    @Test
    public void testCloseIsCalled() throws Exception {
        final Closeable stream = mock(Closeable.class);

        IOUtils.safeClose(stream);

        verify(stream).close();
    }

    @Test
    public void testExceptionIsSwallowed() throws Exception {
        final Closeable stream = mock(Closeable.class);
        doThrow(IOException.class).when(stream).close();

        IOUtils.safeClose(stream);

        verify(stream).close();
    }

    @Test
    public void testNullIsIgnored() {
        IOUtils.safeClose(null);
    }
}