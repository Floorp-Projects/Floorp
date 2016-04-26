/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.util;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.Charset;

import static junit.framework.Assert.*;
import static org.mockito.Mockito.*;

/**
 * Tests the utilities in {@link FileUtils}.
 */
@RunWith(TestRunner.class)
public class TestFileUtils {

    @Rule
    public TemporaryFolder tempDir = new TemporaryFolder();
    public File testFile;

    @Before
    public void setUp() throws Exception {
        testFile = tempDir.newFile();
    }

    @Test
    public void testWriteStringToOutputStreamAndCloseStreamWritesData() throws Exception {
        final String expected = "A string with some data in it! \u00f1 \n";
        final FileOutputStream fos = new FileOutputStream(testFile, false);
        FileUtils.writeStringToOutputStreamAndCloseStream(fos, expected);

        assertTrue("Written file exists", testFile.exists());
        assertEquals("Read data equals written data", expected, readStringFromFile(testFile, expected.length()));
    }

    @Test(expected=IOException.class)
    public void testWriteStringToOutputStreamAndCloseStreamClosesStream() throws Exception {
        final FileOutputStream fos = new FileOutputStream(testFile, false);
        try {
            fos.write('c'); // should not throw because stream is open.
            FileUtils.writeStringToOutputStreamAndCloseStream(fos, "some string with data");
        } catch (final IOException e) {
            fail("Did not expect method to throw when writing file");
        }

        fos.write('c'); // expected to throw because stream is closed.
    }

    /**
     * The Writer we wrap our stream in can throw in .close(), preventing the underlying stream from closing.
     * I added code to prevent ensure we close if the writer .close() throws.
     *
     * I wrote this test to test that code, however, we'd have to mock the writer [1] and that isn't straight-forward.
     * I left this test around because it's a good test of other code.
     *
     * [1]: We thought we could mock FileOutputStream.flush but it's only flushed if the Writer thinks it should be
     * flushed. We can write directly to the Stream, but that doesn't change the Writer state and doesn't affect whether
     * it thinks it should be flushed.
     */
    @Test(expected=IOException.class)
    public void testWriteStringToOutputStreamAndCloseStreamClosesStreamIfWriterThrows() throws Exception {
        final FileOutputStream fos = mock(FileOutputStream.class);
        doThrow(IOException.class).when(fos).write(any(byte[].class), anyInt(), anyInt());
        doThrow(IOException.class).when(fos).write(anyInt());
        doThrow(IOException.class).when(fos).write(any(byte[].class));

        boolean exceptionCaught = false;
        try {
            FileUtils.writeStringToOutputStreamAndCloseStream(fos, "some string with data");
        } catch (final IOException e) {
            exceptionCaught = true;
        }
        assertTrue("Exception caught during tested method", exceptionCaught); // not strictly necessary but documents assumptions

        fos.write('c'); // expected to throw because stream is closed.
    }

    @Test
    public void testWriteStringToFile() throws Exception {
        final String expected = "String to write contains hard characters: !\n \\s..\"'\u00f1";
        FileUtils.writeStringToFile(testFile, expected);

        assertTrue("Written file exists", testFile.exists());
        assertEquals("Read data equals written data", expected, readStringFromFile(testFile, expected.length()));
    }

    // Since the read methods may not be tested yet.
    private static String readStringFromFile(final File file, final int bufferLen) throws IOException {
        final char[] buffer = new char[bufferLen];
        try (InputStreamReader reader = new InputStreamReader(new FileInputStream(file), Charset.forName("UTF-8"))) {
            reader.read(buffer, 0, buffer.length);
        }
        return new String(buffer);
    }
}
