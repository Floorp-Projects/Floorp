/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;

import org.json.JSONObject;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.mozilla.gecko.util.FileUtils;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import java.io.File;
import java.io.IOException;
import java.util.UUID;

import static org.junit.Assert.*;

/**
 * Unit test methods of the GeckoProfile class.
 */
@RunWith(RobolectricTestRunner.class)
public class TestGeckoProfile {
    private static final String PROFILE_NAME = "profileName";

    private static final String CLIENT_ID_JSON_ATTR = "clientID";
    private static final String PROFILE_CREATION_DATE_JSON_ATTR = "created";

    @Rule
    public TemporaryFolder dirContainingProfile = new TemporaryFolder();

    private File profileDir;
    private GeckoProfile profile;

    private File clientIdFile;
    private File timesFile;

    @Before
    public void setUp() throws IOException {
        final Context context = RuntimeEnvironment.application;
        profileDir = dirContainingProfile.newFolder();
        profile = GeckoProfile.get(context, PROFILE_NAME, profileDir);

        clientIdFile = new File(profileDir, "datareporting/state.json");
        timesFile = new File(profileDir, "times.json");
    }

    public void assertValidClientId(final String clientId) {
        // This isn't the method we use in the main GeckoProfile code, but it should be equivalent.
        UUID.fromString(clientId); // assert: will throw if null or invalid UUID.
    }

    @Test
    public void testGetDir() {
        assertEquals("Profile dir argument during construction and returned value are equal",
                profileDir, profile.getDir());
    }

    @Test
    public void testGetClientIdFreshProfile() throws Exception {
        assertFalse("client ID file does not exist", clientIdFile.exists());

        // No existing client ID file: we're expected to create one.
        final String clientId = profile.getClientId();
        assertValidClientId(clientId);
        assertTrue("client ID file exists", clientIdFile.exists());

        assertEquals("Returned client ID is the same as the one previously returned", clientId, profile.getClientId());
        assertEquals("clientID file format matches expectations", clientId, readClientIdFromFile(clientIdFile));
    }

    @Test
    public void testGetClientIdFileAlreadyExists() throws Exception {
        final String validClientId = "905de1c0-0ea6-4a43-95f9-6170035f5a82";
        assertTrue("Created the parent dirs of the client ID file", clientIdFile.getParentFile().mkdirs());
        writeClientIdToFile(clientIdFile, validClientId);

        final String clientIdFromProfile = profile.getClientId();
        assertEquals("Client ID from method matches ID written to disk", validClientId, clientIdFromProfile);
    }

    @Test
    public void testGetClientIdInvalidIdOnDisk() throws Exception {
        assertTrue("Created the parent dirs of the client ID file", clientIdFile.getParentFile().mkdirs());
        writeClientIdToFile(clientIdFile, "");
        final String clientIdForEmptyString = profile.getClientId();
        assertValidClientId(clientIdForEmptyString);
        assertNotEquals("A new client ID was created when the empty String was written to disk", "", clientIdForEmptyString);

        writeClientIdToFile(clientIdFile, "invalidClientId");
        final String clientIdForInvalidClientId = profile.getClientId();
        assertValidClientId(clientIdForInvalidClientId);
        assertNotEquals("A new client ID was created when an invalid client ID was written to disk",
                "invalidClientId", clientIdForInvalidClientId);
    }

    @Test
    public void testGetClientIdMissingClientIdJSONAttr() throws Exception {
        final String validClientId = "905de1c0-0ea6-4a43-95f9-6170035f5a82";
        final JSONObject objMissingClientId = new JSONObject();
        objMissingClientId.put("irrelevantKey", validClientId);
        assertTrue("Created the parent dirs of the client ID file", clientIdFile.getParentFile().mkdirs());
        FileUtils.writeJSONObjectToFile(clientIdFile, objMissingClientId);

        final String clientIdForMissingAttr = profile.getClientId();
        assertValidClientId(clientIdForMissingAttr);
        assertNotEquals("Did not use other attr when JSON attr was missing", validClientId, clientIdForMissingAttr);
    }

    @Test
    public void testGetClientIdInvalidIdFileFormat() throws Exception {
        final String validClientId = "905de1c0-0ea6-4a43-95f9-6170035f5a82";
        assertTrue("Created the parent dirs of the client ID file", clientIdFile.getParentFile().mkdirs());
        FileUtils.writeStringToFile(clientIdFile, "clientID: \"" + validClientId + "\"");

        final String clientIdForInvalidFormat = profile.getClientId();
        assertValidClientId(clientIdForInvalidFormat);
        assertNotEquals("Created new ID when file format was invalid", validClientId, clientIdForInvalidFormat);
    }

    @Test
    public void testEnsureParentDirs() {
        final File grandParentDir = new File(profileDir, "grandParent");
        final File parentDir = new File(grandParentDir, "parent");
        final File childFile = new File(parentDir, "child");

        // Assert initial state.
        assertFalse("Topmost parent dir should not exist yet", grandParentDir.exists());
        assertFalse("Bottommost parent dir should not exist yet", parentDir.exists());
        assertFalse("Child file should not exist", childFile.exists());

        final String fakeFullPath = "grandParent/parent/child";
        assertTrue("Parent directories should be created", profile.ensureParentDirs(fakeFullPath));
        assertTrue("Topmost parent dir should have been created", grandParentDir.exists());
        assertTrue("Bottommost parent dir should have been created", parentDir.exists());
        assertFalse("Child file should not have been created", childFile.exists());

        // Parents already exist because this is the second time we're calling ensureParentDirs.
        assertTrue("Expect true if parent directories already exist", profile.ensureParentDirs(fakeFullPath));

        // Assert error condition.
        assertTrue("Ensure we can change permissions on profile dir for testing", profileDir.setReadOnly());
        assertFalse("Expect false if the parent dir could not be created", profile.ensureParentDirs("unwritableDir/child"));
    }

    @Test
    public void testIsClientIdValid() {
        final String[] validClientIds = new String[] {
                "905de1c0-0ea6-4a43-95f9-6170035f5a82",
                "905de1c0-0ea6-4a43-95f9-6170035f5a83",
                "57472f82-453d-4c55-b59c-d3c0e97b76a1",
                "895745d1-f31e-46c3-880e-b4dd72963d4f",
        };
        for (final String validClientId : validClientIds) {
            assertTrue("Client ID, " + validClientId + ", is valid", GeckoProfile.isClientIdValid(validClientId));
        }

        final String[] invalidClientIds = new String[] {
                null,
                "",
                "a",
                "anInvalidClientId",
                "905de1c0-0ea6-4a43-95f9-6170035f5a820", // too long (last section)
                "905de1c0-0ea6-4a43-95f9-6170035f5a8", // too short (last section)
                "05de1c0-0ea6-4a43-95f9-6170035f5a82", // too short (first section)
                "905de1c0-0ea6-4a43-95f9-6170035f5a8!", // contains a symbol
        };
        for (final String invalidClientId : invalidClientIds) {
            assertFalse("Client ID, " + invalidClientId + ", is invalid", GeckoProfile.isClientIdValid(invalidClientId));
        }

        // We generate client IDs using UUID - better make sure they're valid.
        for (int i = 0; i < 30; ++i) {
            final String generatedClientId = UUID.randomUUID().toString();
            assertTrue("Generated client ID from UUID, " + generatedClientId + ", is valid",
                    GeckoProfile.isClientIdValid(generatedClientId));
        }
    }

    @Test
    public void testGetProfileCreationDateFromTimesFile() throws Exception {
        final long expectedDate = System.currentTimeMillis();
        final JSONObject expectedObj = new JSONObject();
        expectedObj.put(PROFILE_CREATION_DATE_JSON_ATTR, expectedDate);
        FileUtils.writeJSONObjectToFile(timesFile, expectedObj);

        final Context context = RuntimeEnvironment.application;
        final long actualDate = profile.getAndPersistProfileCreationDate(context);
        assertEquals("Date from disk equals date inserted to disk", expectedDate, actualDate);

        final long actualDateFromDisk = readProfileCreationDateFromFile(timesFile);
        assertEquals("Date in times.json has not changed after accessing profile creation date",
                expectedDate, actualDateFromDisk);
    }

    @Test
    public void testGetProfileCreationDateTimesFileDoesNotExist() throws Exception {
        assertFalse("Times.json does not already exist", timesFile.exists());

        final Context context = RuntimeEnvironment.application;
        final long actualDate = profile.getAndPersistProfileCreationDate(context);
        // I'd prefer to mock so we can return and verify a specific value but we can't mock
        // GeckoProfile because it's final. Instead, we check if the value is at least reasonable.
        assertTrue("Date from method is positive", actualDate >= 0);
        assertTrue("Date from method is less than current time", actualDate < System.currentTimeMillis());

        assertTrue("Times.json exists after getting profile", timesFile.exists());
        final long actualDateFromDisk = readProfileCreationDateFromFile(timesFile);
        assertEquals("Date from disk equals returned value", actualDate, actualDateFromDisk);
    }

    private static long readProfileCreationDateFromFile(final File file) throws Exception {
        final JSONObject actualObj = FileUtils.readJSONObjectFromFile(file);
        return actualObj.getLong(PROFILE_CREATION_DATE_JSON_ATTR);
    }

    private String readClientIdFromFile(final File file) throws Exception {
        final JSONObject obj = FileUtils.readJSONObjectFromFile(file);
        return obj.getString(CLIENT_ID_JSON_ATTR);
    }

    private void writeClientIdToFile(final File file, final String clientId) throws Exception {
        final JSONObject obj = new JSONObject();
        obj.put(CLIENT_ID_JSON_ATTR, clientId);
        FileUtils.writeJSONObjectToFile(file, obj);
    }
}
