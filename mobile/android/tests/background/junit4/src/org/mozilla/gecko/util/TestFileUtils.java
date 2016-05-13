/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.util;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.util.FileUtils.FileLastModifiedComparator;
import org.mozilla.gecko.util.FileUtils.FilenameRegexFilter;
import org.mozilla.gecko.util.FileUtils.FilenameWhitelistFilter;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.nio.charset.Charset;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;
import java.util.regex.Pattern;

import static junit.framework.Assert.*;
import static org.mockito.Mockito.*;

/**
 * Tests the utilities in {@link FileUtils}.
 */
@RunWith(TestRunner.class)
public class TestFileUtils {

    private static final Charset CHARSET = Charset.forName("UTF-8");

    @Rule
    public TemporaryFolder tempDir = new TemporaryFolder();
    public File testFile;
    public File nonExistentFile;

    @Before
    public void setUp() throws Exception {
        testFile = tempDir.newFile();
        nonExistentFile = new File(tempDir.getRoot(), "non-existent-file");
    }

    @Test
    public void testReadJSONObjectFromFile() throws Exception {
        final JSONObject expected = new JSONObject("{\"str\": \"some str\"}");
        writeStringToFile(testFile, expected.toString());

        final JSONObject actual = FileUtils.readJSONObjectFromFile(testFile);
        assertEquals("JSON contains expected str", expected.getString("str"), actual.getString("str"));
    }

    @Test(expected=IOException.class)
    public void testReadJSONObjectFromFileEmptyFile() throws Exception {
        assertEquals("Test file is empty", 0, testFile.length());
        FileUtils.readJSONObjectFromFile(testFile); // expected to throw
    }

    @Test(expected=JSONException.class)
    public void testReadJSONObjectFromFileInvalidJSON() throws Exception {
        writeStringToFile(testFile, "not a json str");
        FileUtils.readJSONObjectFromFile(testFile); // expected to throw
    }

    @Test
    public void testReadStringFromFileReadsData() throws Exception {
        final String expected = "String to write contains hard characters: !\n \\s..\"'\u00f1";
        writeStringToFile(testFile, expected);

        final String actual = FileUtils.readStringFromFile(testFile);
        assertEquals("Read content matches written content", expected, actual);
    }

    @Test
    public void testReadStringFromFileEmptyFile() throws Exception {
        assertEquals("Test file is empty", 0, testFile.length());

        final String actual = FileUtils.readStringFromFile(testFile);
        assertEquals("Read content is empty", "", actual);
    }

    @Test(expected=FileNotFoundException.class)
    public void testReadStringFromNonExistentFile() throws Exception {
        assertFalse("File does not exist", nonExistentFile.exists());
        FileUtils.readStringFromFile(nonExistentFile);
    }

    @Test
    public void testReadStringFromInputStreamAndCloseStreamBufferLenIsFileLen() throws Exception {
        final String expected = "String to write contains hard characters: !\n \\s..\"'\u00f1";
        writeStringToFile(testFile, expected);

        final FileInputStream stream = new FileInputStream(testFile);
        final String actual = FileUtils.readStringFromInputStreamAndCloseStream(stream, expected.length());
        assertEquals("Read content matches written content", expected, actual);
    }

    @Test
    public void testReadStringFromInputStreamAndCloseStreamBufferLenIsBiggerThanFile() throws Exception {
        final String expected = "aoeuhtns";
        writeStringToFile(testFile, expected);

        final FileInputStream stream = new FileInputStream(testFile);
        final String actual = FileUtils.readStringFromInputStreamAndCloseStream(stream, expected.length() + 1024);
        assertEquals("Read content matches written content", expected, actual);
    }

    @Test
    public void testReadStringFromInputStreamAndCloseStreamBufferLenIsSmallerThanFile() throws Exception {
        final String expected = "aoeuhtns aoeusth aoeusth aoeusnth aoeusth aoeusnth aoesuth";
        writeStringToFile(testFile, expected);

        final FileInputStream stream = new FileInputStream(testFile);
        final String actual = FileUtils.readStringFromInputStreamAndCloseStream(stream, 8);
        assertEquals("Read content matches written content", expected, actual);
    }

    @Test(expected=IllegalArgumentException.class)
    public void testReadStringFromInputStreamAndCloseStreamBufferLenIsZero() throws Exception {
        final String expected = "aoeuhtns aoeusth aoeusth aoeusnth aoeusth aoeusnth aoesuth";
        writeStringToFile(testFile, expected);

        final FileInputStream stream = new FileInputStream(testFile);
        FileUtils.readStringFromInputStreamAndCloseStream(stream, 0); // expected to throw.
    }

    @Test
    public void testReadStringFromInputStreamAndCloseStreamIsEmptyStream() throws Exception {
        assertTrue("Test file exists", testFile.exists());
        assertEquals("Test file is empty", 0, testFile.length());

        final FileInputStream stream = new FileInputStream(testFile);
        final String actual = FileUtils.readStringFromInputStreamAndCloseStream(stream, 8);
        assertEquals("Read content from stream is empty", "", actual);
    }

    @Test(expected=IOException.class)
    public void testReadStringFromInputStreamAndCloseStreamClosesStream() throws Exception {
        final String expected = "String to write contains hard characters: !\n \\s..\"'\u00f1";
        writeStringToFile(testFile, expected);

        final FileInputStream stream = new FileInputStream(testFile);
        try {
            stream.read(); // should not throw because stream is open.
            FileUtils.readStringFromInputStreamAndCloseStream(stream, expected.length());
        } catch (final IOException e) {
            fail("Did not expect method to throw when writing file: " + e);
        }

        stream.read(); // expected to throw because stream is closed.
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
            fail("Did not expect method to throw when writing file: " + e);
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

    @Test
    public void testWriteStringToFileEmptyString() throws Exception {
        final String expected = "";
        FileUtils.writeStringToFile(testFile, expected);

        assertTrue("Written file exists", testFile.exists());
        assertEquals("Written file is empty", 0, testFile.length());
        assertEquals("Read data equals written (empty) data", expected, readStringFromFile(testFile, expected.length()));
    }

    @Test
    public void testWriteStringToFileCreatesNewFile() throws Exception {
        final String expected = "some str to write";
        assertFalse("Non existent file does not exist", nonExistentFile.exists());
        FileUtils.writeStringToFile(nonExistentFile, expected); // expected to create file

        assertTrue("Written file was created", nonExistentFile.exists());
        assertEquals("Read data equals written data", expected, readStringFromFile(nonExistentFile, (int) nonExistentFile.length()));
    }

    @Test
    public void testWriteStringToFileOverwritesFile() throws Exception {
        writeStringToFile(testFile, "data");

        final String expected = "some str to write";
        FileUtils.writeStringToFile(testFile, expected);

        assertTrue("Written file was created", testFile.exists());
        assertEquals("Read data equals written data", expected, readStringFromFile(testFile, (int) testFile.length()));
    }

    @Test
    public void testWriteJSONObjectToFile() throws Exception {
        final JSONObject expected = new JSONObject()
                .put("int", 1)
                .put("str", "1")
                .put("bool", true)
                .put("null", JSONObject.NULL)
                .put("raw null", null);
        FileUtils.writeJSONObjectToFile(testFile, expected);

        assertTrue("Written file exists", testFile.exists());

        // JSONObject.equals compares references so we have to assert each key individually. >:(
        final JSONObject actual = new JSONObject(readStringFromFile(testFile, (int) testFile.length()));
        assertEquals(1, actual.getInt("int"));
        assertEquals("1", actual.getString("str"));
        assertEquals(true, actual.getBoolean("bool"));
        assertEquals(JSONObject.NULL, actual.get("null"));
        assertFalse(actual.has("raw null"));
    }

    // Since the read methods may not be tested yet.
    private static String readStringFromFile(final File file, final int bufferLen) throws IOException {
        final char[] buffer = new char[bufferLen];
        try (InputStreamReader reader = new InputStreamReader(new FileInputStream(file), Charset.forName("UTF-8"))) {
            reader.read(buffer, 0, buffer.length);
        }
        return new String(buffer);
    }

    // Since the write methods may not be tested yet.
    private static void writeStringToFile(final File file, final String str) throws IOException {
        try (OutputStreamWriter writer = new OutputStreamWriter(new FileOutputStream(file, false), CHARSET)) {
            writer.write(str);
        }
        assertTrue("Written file from helper method exists", file.exists());
    }

    @Test
    public void testFilenameWhitelistFilter() {
        final String[] expectedToAccept = new String[] { "one", "two", "three" };
        final Set<String> whitelist = new HashSet<>(Arrays.asList(expectedToAccept));
        final FilenameWhitelistFilter testFilter = new FilenameWhitelistFilter(whitelist);
        for (final String str : expectedToAccept) {
            assertTrue("Filename, " + str + ", in whitelist is accepted", testFilter.accept(testFile, str));
        }

        final String[] notExpectedToAccept = new String[] { "not-in-whitelist", "meh", "whatever" };
        for (final String str : notExpectedToAccept) {
            assertFalse("Filename, " + str + ", not in whitelist is not accepted", testFilter.accept(testFile, str));
        }
    }

    @Test
    public void testFilenameRegexFilter() {
        final Pattern pattern = Pattern.compile("[a-z]{1,6}");
        final FilenameRegexFilter testFilter = new FilenameRegexFilter(pattern);
        final String[] expectedToAccept = new String[] { "duckie", "goes", "quack" };
        for (final String str : expectedToAccept) {
            assertTrue("Filename, " + str + ", matching regex expected to accept", testFilter.accept(testFile, str));
        }

        final String[] notExpectedToAccept = new String[] { "DUCKIE", "1337", "2fast" };
        for (final String str : notExpectedToAccept) {
            assertFalse("Filename, " + str + ", not matching regex not expected to accept", testFilter.accept(testFile, str));
        }
    }

    @Test
    public void testFileLastModifiedComparator() {
        final FileLastModifiedComparator testComparator = new FileLastModifiedComparator();
        final File oldFile = mock(File.class);
        final File newFile = mock(File.class);
        final File equallyNewFile = mock(File.class);
        when(oldFile.lastModified()).thenReturn(10L);
        when(newFile.lastModified()).thenReturn(100L);
        when(equallyNewFile.lastModified()).thenReturn(100L);

        assertTrue("Old file is less than new file", testComparator.compare(oldFile, newFile) < 0);
        assertTrue("New file is greater than old file", testComparator.compare(newFile, oldFile) > 0);
        assertTrue("New files are equal", testComparator.compare(newFile, equallyNewFile) == 0);
    }
}
