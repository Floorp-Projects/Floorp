/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.util;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import java.io.File;
import java.util.UUID;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

@RunWith(RobolectricTestRunner.class)
public class FileUtilsTest {
    @Test
    public void testAssertDirectory() {
        final File folder = new File(RuntimeEnvironment.application.getCacheDir(), UUID.randomUUID().toString());
        FileUtils.assertDirectory(folder);
    }

    @SuppressWarnings("ResultOfMethodCallIgnored")
    @Test(expected = IllegalStateException.class)
    public void testThrowsIfDirectoryCannotBeCreated() {
        final File file = mock(File.class);
        doReturn(false).when(file).exists();
        doReturn(false).when(file).mkdirs();

        FileUtils.assertDirectory(file);
    }

    @SuppressWarnings("ResultOfMethodCallIgnored")
    @Test(expected = IllegalStateException.class)
    public void testThrowsIfFileIsNotDirectory() {
        final File file = mock(File.class);
        doReturn(true).when(file).exists();
        doReturn(true).when(file).mkdirs();
        doReturn(false).when(file).isDirectory();

        FileUtils.assertDirectory(file);
    }

    @SuppressWarnings("ResultOfMethodCallIgnored")
    @Test(expected = IllegalStateException.class)
    public void testThrowsIfDirectoryIsNotWritable() {
        final File file = mock(File.class);
        doReturn(true).when(file).exists();
        doReturn(true).when(file).mkdirs();
        doReturn(true).when(file).isDirectory();
        doReturn(false).when(file).canWrite();

        FileUtils.assertDirectory(file);
    }
}