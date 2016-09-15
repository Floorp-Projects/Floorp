/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.*;

import android.util.Log;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.GeckoProfile;

import java.io.File;
import java.io.IOException;
import java.util.UUID;

public class testUnifiedTelemetryClientId extends JavascriptBridgeTest {
    private static final String TEST_JS = "testUnifiedTelemetryClientId.js";

    private static final String CLIENT_ID_PATH = "datareporting/state.json";
    private static final String FHR_DIR_PATH = "healthreport/";
    private static final String FHR_CLIENT_ID_PATH = FHR_DIR_PATH + "state.json";

    private GeckoProfile profile;
    private File profileDir;
    private File[] filesToDeleteOnReset;

    public void setUp() throws Exception {
        super.setUp();
        profile = getTestProfile();
        profileDir = profile.getDir(); // Assumes getDir is tested.
        filesToDeleteOnReset = new File[] {
                getClientIdFile(),
                getFHRClientIdFile(),
                getFHRClientIdParentDir(),
        };
    }

    public void tearDown() throws Exception {
        // Don't clear cache because who knows what state Gecko is in.
        deleteClientIDFiles();
        super.tearDown();
    }

    private void deleteClientIDFiles() {
        Log.d(LOGTAG, "deleteClientIDFiles: begin");

        for (final File file : filesToDeleteOnReset) {
            file.delete(); // can't check return value because the file may not exist before deletion.
            fAssertFalse("Deleted file in reset does not exist", file.exists()); // sanity check.
        }

        Log.d(LOGTAG, "deleteClientIDFiles: end");
    }

    public void testUnifiedTelemetryClientId() throws Exception {
        blockForReadyAndLoadJS(TEST_JS);
        fAssertTrue("Profile directory exists", profileDir.exists());

        // Important note: we cannot stop Gecko from running while we run this test and
        // Gecko is capable of creating client ID files while we run this test. However,
        // ClientID.jsm will not touch modify the client ID files on disk if its client
        // ID cache is filled. As such, we prevent it from touching the disk by intentionally
        // priming the cache & deleting the files it added now, and resetting the cache at the
        // latest possible moment before we attempt to test the client ID file.
        //
        // This is fragile because it relies on the ClientID cache's implementation, however,
        // some alternatives (e.g. changing file system permissions, file locking) are worse
        // because they can fire error handling code, which is not currently under test.
        //
        // First, we delete the test files - we don't want the cache prime to fail which could happen if
        // these files are around & corrupted from a previous test/install. Then we prime the cache,
        // and delete the files the cache priming added, so the tests are ready to add their own version
        // of these files.
        deleteClientIDFiles();
        primeJsClientIdCache();
        deleteClientIDFiles();

        // TODO: If these tests weren't so expensive to run in automation,
        // this should be two separate tests to avoid storing state between tests.
        testJavaCreatesClientId(); // leaves cache filled.
        deleteClientIDFiles();
        testJsCreatesClientId(); // leaves cache filled.
        deleteClientIDFiles();
        testJavaMigratesFromHealthReport(); // leaves cache filled.
        deleteClientIDFiles();
        testJsMigratesFromHealthReport(); // leaves cache filled.

        getJS().syncCall("endTest");
    }

    /**
     * Scenario: Java creates client ID:
     *   * Fennec starts on fresh profile
     *   * Java code creates the client ID in datareporting/state.json
     *   * Js accesses client ID from the same file
     *   * Assert the client IDs are the same
     */
    private void testJavaCreatesClientId() throws Exception {
        Log.d(LOGTAG, "testJavaCreatesClientId: start");

        fAssertFalse("Client id file does not exist yet", getClientIdFile().exists());

        final String clientIdFromJava = getClientIdFromJava();
        resetJSCache();
        final String clientIdFromJS = getClientIdFromJS();
        // allow for the case where gecko updates the client ID after the first get
        final String clientIdFromJavaAgain = getClientIdFromJava();
        fAssertTrue("Client ID from Java equals ID from JS",
            clientIdFromJava.equals(clientIdFromJS) ||
            clientIdFromJavaAgain.equals(clientIdFromJS));

        final String clientIdFromJSCache = getClientIdFromJS();
        resetJSCache();
        final String clientIdFromJSFileAgain = getClientIdFromJS();
        fAssertEquals("Same client ID retrieved from JS cache", clientIdFromJavaAgain, clientIdFromJSCache);
        fAssertEquals("Same client ID retrieved from JS file", clientIdFromJavaAgain, clientIdFromJSFileAgain);
    }

    /**
     * Scenario: JS creates client ID
     *   * Fennec starts on a fresh profile
     *   * Js creates the client ID in datareporting/state.json
     *   * Java access the client ID from the same file
     *   * Assert the client IDs are the same
     */
    private void testJsCreatesClientId() throws Exception {
        Log.d(LOGTAG, "testJsCreatesClientId: start");

        fAssertFalse("Client id file does not exist yet", getClientIdFile().exists());

        resetJSCache();
        final String clientIdFromJS = getClientIdFromJS();
        final String clientIdFromJava = getClientIdFromJava();
        fAssertEquals("Client ID from JS equals ID from Java", clientIdFromJS, clientIdFromJava);

        final String clientIdFromJSCache = getClientIdFromJS();
        final String clientIdFromJavaAgain = getClientIdFromJava();
        resetJSCache();
        final String clientIdFromJSFileAgain = getClientIdFromJS();
        fAssertEquals("Same client ID retrieved from JS cache", clientIdFromJS, clientIdFromJSCache);
        fAssertEquals("Same client ID retrieved from JS file", clientIdFromJS, clientIdFromJSFileAgain);
        fAssertEquals("Same client ID retrieved from Java", clientIdFromJS, clientIdFromJavaAgain);
    }

    /**
     * Scenario: Java migrates client ID from FHR client ID file.
     *   * FHR file already exists.
     *   * Fennec starts on fresh profile
     *   * Java code merges client ID to datareporting/state.json from healthreport/state.json
     *   * Js accesses client ID from the same file
     *   * Assert the client IDs are the same
     */
    private void testJavaMigratesFromHealthReport() throws Exception {
        Log.d(LOGTAG, "testJavaMigratesFromHealthReport: start");

        fAssertFalse("Client id file does not exist yet", getClientIdFile().exists());
        fAssertFalse("Health report file does not exist yet", getFHRClientIdFile().exists());

        final String expectedClientId = UUID.randomUUID().toString();
        createFHRClientIdFile(expectedClientId);

        final String clientIdFromJava = getClientIdFromJava();
        fAssertEquals("Health report client ID merged by Java", expectedClientId, clientIdFromJava);
        resetJSCache();
        final String clientIdFromJS = getClientIdFromJS();
        fAssertEquals("Merged client ID read by JS", expectedClientId, clientIdFromJS);

        final String clientIdFromJavaAgain = getClientIdFromJava();
        final String clientIdFromJSCache = getClientIdFromJS();
        resetJSCache();
        final String clientIdFromJSFileAgain = getClientIdFromJS();
        fAssertEquals("Same client ID retrieved from Java", expectedClientId, clientIdFromJavaAgain);
        fAssertEquals("Same client ID retrieved from JS cache", expectedClientId, clientIdFromJSCache);
        fAssertEquals("Same client ID retrieved from JS file", expectedClientId, clientIdFromJSFileAgain);
    }

    /**
     * Scenario: JS merges client ID from FHR client ID file.
     *   * FHR file already exists.
     *   * Fennec starts on a fresh profile
     *   * Js merges the client ID to datareporting/state.json from healthreport/state.json
     *   * Java access the client ID from the same file
     *   * Assert the client IDs are the same
     */
    private void testJsMigratesFromHealthReport() throws Exception {
        Log.d(LOGTAG, "testJsMigratesFromHealthReport: start");

        fAssertFalse("Client id file does not exist yet", getClientIdFile().exists());
        fAssertFalse("Health report file does not exist yet", getFHRClientIdFile().exists());

        final String expectedClientId = UUID.randomUUID().toString();
        createFHRClientIdFile(expectedClientId);

        resetJSCache();
        final String clientIdFromJS = getClientIdFromJS();
        fAssertEquals("Health report client ID merged by JS", expectedClientId, clientIdFromJS);
        final String clientIdFromJava = getClientIdFromJava();
        fAssertEquals("Merged client ID read by Java", expectedClientId, clientIdFromJava);

        final String clientIdFromJavaAgain = getClientIdFromJava();
        final String clientIdFromJSCache = getClientIdFromJS();
        resetJSCache();
        final String clientIdFromJSFileAgain = getClientIdFromJS();
        fAssertEquals("Same client ID retrieved from Java", expectedClientId, clientIdFromJavaAgain);
        fAssertEquals("Same client ID retrieved from JS cache", expectedClientId, clientIdFromJSCache);
        fAssertEquals("Same client ID retrieved from JS file", expectedClientId, clientIdFromJSFileAgain);
    }

    private String getClientIdFromJava() throws IOException {
        // This assumes implementation details: it assumes the client ID
        // file is created when Java attempts to retrieve it if it does not exist.
        final String clientId = profile.getClientId();
        fAssertNotNull("Returned client ID is not null", clientId);
        fAssertTrue("Client ID file exists after getClientId call", getClientIdFile().exists());
        return clientId;
    }

    private String getClientIdFromJS() {
        return getBlockingFromJsString("clientId");
    }

    /**
     * Must be called after Gecko is loaded.
     */
    private void primeJsClientIdCache() {
        // Not the cleanest way, but it works.
        getClientIdFromJS();
    }

    /**
     * Resets the client ID cache in ClientID.jsm. This method *must* be called after
     * Gecko is loaded or else this method will hang.
     *
     * Note: we do this for very specific reasons - see the comment in the test method
     * ({@link #testUnifiedTelemetryClientId()}) for more.
     */
    private void resetJSCache() {
        // HACK: the backing JS method is a promise with no return value. Rather than writing a method
        // to handle this (for time reasons), I call the get String method and don't access the return value.
        getBlockingFromJsString("reset");
    }

    private File getClientIdFile() {
        return new File(profileDir, CLIENT_ID_PATH);
    }

    private File getFHRClientIdParentDir() {
        return new File(profileDir, FHR_DIR_PATH);
    }

    private File getFHRClientIdFile() {
        return new File(profileDir, FHR_CLIENT_ID_PATH);
    }

    private void createFHRClientIdFile(final String clientId) throws JSONException {
        fAssertTrue("FHR directory created", getFHRClientIdParentDir().mkdirs());

        final JSONObject obj = new JSONObject();
        obj.put("clientID", clientId);
        profile.writeFile(FHR_CLIENT_ID_PATH, obj.toString());
        fAssertTrue("FHR client ID file exists after writing", getFHRClientIdFile().exists());
    }
}
