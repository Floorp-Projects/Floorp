/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.telemetry.stores;

import org.json.JSONObject;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.telemetry.TelemetryPing;
import org.mozilla.gecko.util.FileUtils;

import java.io.File;
import java.io.FileOutputStream;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.UUID;

import static org.junit.Assert.*;

/**
 * Unit test methods of the {@link TelemetryJSONFilePingStore} class.
 */
@RunWith(TestRunner.class)
public class TestTelemetryJSONFilePingStore {

    @Rule
    public TemporaryFolder tempDir = new TemporaryFolder();
    private File testDir;
    private TelemetryJSONFilePingStore testStore;

    @Before
    public void setUp() throws Exception {
        testDir = tempDir.newFolder();
        testStore = new TelemetryJSONFilePingStore(testDir);
    }

    private ExtendedJSONObject generateTelemetryPayload() {
        final ExtendedJSONObject out = new ExtendedJSONObject();
        out.put("str", "a String");
        out.put("int", 42);
        out.put("null", (ExtendedJSONObject) null);
        return out;
    }

    private void assertIsGeneratedPayload(final ExtendedJSONObject actual) throws Exception {
        assertNull("Null field is null", actual.getObject("null"));
        assertEquals("int field is correct", 42, (int) actual.getIntegerSafely("int"));
        assertEquals("str field is correct", "a String", actual.getString("str"));
    }

    private void assertStoreFileCount(final int expectedCount) {
        assertEquals("Store contains " + expectedCount + " item(s)", expectedCount, testDir.list().length);
    }

    @Test
    public void testConstructorOnlyWritesToGivenDir() throws Exception {
        // Constructor is called in @Before method
        assertTrue("Store dir exists", testDir.exists());
        assertEquals("Temp dir contains one dir (the store dir)", 1, tempDir.getRoot().list().length);
    }

    @Test
    public void testStorePingStoresCorrectData() throws Exception {
        assertStoreFileCount(0);

        final String expectedID = getDocID();
        final TelemetryPing expectedPing = new TelemetryPing("a/server/url", generateTelemetryPayload(), expectedID);
        testStore.storePing(expectedPing);

        assertStoreFileCount(1);
        final String filename = testDir.list()[0];
        assertTrue("Filename contains expected ID", filename.equals(expectedID));
        final JSONObject actual = FileUtils.readJSONObjectFromFile(new File(testDir, filename));
        assertEquals("Ping url paths are equal", expectedPing.getURLPath(), actual.getString(TelemetryJSONFilePingStore.KEY_URL_PATH));
        assertIsGeneratedPayload(new ExtendedJSONObject(actual.getString(TelemetryJSONFilePingStore.KEY_PAYLOAD)));
    }

    @Test
    public void testStorePingMultiplePingsStoreSeparateFiles() throws Exception {
        assertStoreFileCount(0);
        for (int i = 1; i < 10; ++i) {
            testStore.storePing(new TelemetryPing("server", generateTelemetryPayload(), getDocID()));
            assertStoreFileCount(i);
        }
    }

    @Test
    public void testStorePingReleasesFileLock() throws Exception {
        assertStoreFileCount(0);
        testStore.storePing(new TelemetryPing("server", generateTelemetryPayload(), getDocID()));
        assertStoreFileCount(1);
        final File file = new File(testDir, testDir.list()[0]);
        final FileOutputStream stream = new FileOutputStream(file);
        try {
            assertNotNull("File lock is released after store write", stream.getChannel().tryLock());
        } finally {
            stream.close(); // releases lock
        }
    }

    @Test
    public void testGetAllPings() throws Exception {
        final String urlPrefix = "url";
        writeTestPingsToStore(3, urlPrefix);

        final ArrayList<TelemetryPing> pings = testStore.getAllPings();
        for (final TelemetryPing ping : pings) {
            assertEquals("Expected url path value received", urlPrefix + ping.getDocID(), ping.getURLPath());
            assertIsGeneratedPayload(ping.getPayload());
        }
    }

    @Test // regression test: bug 1272817
    public void testGetAllPingsHandlesEmptyFiles() throws Exception {
        final int expectedPingCount = 3;
        writeTestPingsToStore(expectedPingCount, "whatever");
        assertTrue("Empty file is created", testStore.getPingFile(getDocID()).createNewFile());
        assertEquals("Returned pings only contains valid files", expectedPingCount, testStore.getAllPings().size());
    }

    @Test
    public void testMaybePrunePingsDoesNothingIfAtMax() throws Exception {
        final int pingCount = TelemetryJSONFilePingStore.MAX_PING_COUNT;
        writeTestPingsToStore(pingCount, "whatever");
        assertStoreFileCount(pingCount);
        testStore.maybePrunePings();
        assertStoreFileCount(pingCount);
    }

    @Test
    public void testMaybePrunePingsPrunesIfAboveMax() throws Exception {
        final int pingCount = TelemetryJSONFilePingStore.MAX_PING_COUNT + 1;
        final List<String> expectedDocIDs = writeTestPingsToStore(pingCount, "whatever");
        assertStoreFileCount(pingCount);
        testStore.maybePrunePings();
        assertStoreFileCount(TelemetryJSONFilePingStore.MAX_PING_COUNT);

        final HashSet<String> existingIDs = new HashSet<>(TelemetryJSONFilePingStore.MAX_PING_COUNT);
        Collections.addAll(existingIDs, testDir.list());
        assertFalse("Oldest ping was removed", existingIDs.contains(expectedDocIDs.get(0)));
    }

    @Test
    public void testOnUploadAttemptCompleted() throws Exception {
        final List<String> savedDocIDs = writeTestPingsToStore(10, "url");
        final int halfSize = savedDocIDs.size() / 2;
        final Set<String> unuploadedPingIDs = new HashSet<>(savedDocIDs.subList(0, halfSize));
        final Set<String> removedPingIDs = new HashSet<>(savedDocIDs.subList(halfSize, savedDocIDs.size()));
        testStore.onUploadAttemptComplete(removedPingIDs);

        for (final String unuploadedDocID : testDir.list()) {
            assertFalse("Unuploaded ID is not in removed ping IDs", removedPingIDs.contains(unuploadedDocID));
            assertTrue("Unuploaded ID is in unuploaded ping IDs", unuploadedPingIDs.contains(unuploadedDocID));
            unuploadedPingIDs.remove(unuploadedDocID);
        }
        assertTrue("All non-successful-upload ping IDs were matched", unuploadedPingIDs.isEmpty());
    }

    @Test
    public void testGetPingFileIsDocID() throws Exception {
        final String docID = getDocID();
        final File file = testStore.getPingFile(docID);
        assertTrue("Ping filename contains ID", file.getName().equals(docID));
    }

    /**
     * Writes pings to store without using store API with:
     *   server = urlPrefix + docID
     *   payload = generated payload
     *
     * The docID is stored as the filename.
     *
     * Note: assumes {@link TelemetryJSONFilePingStore#getPingFile(String)} works.
     *
     * @return a list of doc IDs saved to disk in ascending order of last modified date
     */
    private List<String> writeTestPingsToStore(final int count, final String urlPrefix) throws Exception {
        final List<String> savedDocIDs = new ArrayList<>(count);
        final long now = System.currentTimeMillis();
        for (int i = 1; i <= count; ++i) {
            final String docID = getDocID();
            final JSONObject obj = new JSONObject()
                    .put(TelemetryJSONFilePingStore.KEY_URL_PATH, urlPrefix + docID)
                    .put(TelemetryJSONFilePingStore.KEY_PAYLOAD, generateTelemetryPayload());
            final File pingFile = testStore.getPingFile(docID);
            FileUtils.writeJSONObjectToFile(pingFile, obj);

            // If we don't set an explicit time, the modified times are all equal.
            // Also, last modified times are rounded by second.
            assertTrue("Able to set last modified time", pingFile.setLastModified(now - (count * 10_000) + i * 10_000));
            savedDocIDs.add(docID);
        }
        return savedDocIDs;
    }

    private String getDocID() {
        return UUID.randomUUID().toString();
    }
}
