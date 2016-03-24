/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.*;

import org.mozilla.gecko.GeckoProfile;

import java.io.File;
import java.io.IOException;

public class testUnifiedTelemetryClientId extends JavascriptBridgeTest {
    private static final String TEST_JS = "testUnifiedTelemetryClientId.js";

    private static final String CLIENT_ID_PATH = "datareporting/state.json";

    private GeckoProfile profile;
    private File profileDir;

    public void setUp() throws Exception {
        super.setUp();
        profile = getTestProfile();
        profileDir = profile.getDir(); // Assumes getDir is tested.

        // In local testing, it's possible to ^C out of the harness and not have tearDown called,
        // hence reset. We can't clear the cache because Gecko is not running yet.
        resetTest(false);
    }

    public void tearDown() throws Exception {
        // Don't clear cache because who knows what state Gecko is in.
        resetTest(false);
        getClientIdFile().delete();
        super.tearDown();
    }

    private void resetTest(final boolean resetJSCache) {
        if (resetJSCache) {
            resetJSCache();
        }
        getClientIdFile().delete();
    }

    // TODO: If the intent service runs in the background, it could break this test. The service is disabled
    // on non-official builds (e.g. this one) but that may not be the case on TBPL.
    public void testUnifiedTelemetryClientId() throws Exception {
        blockForReadyAndLoadJS(TEST_JS);
        resetJSCache(); // Must be called after Gecko is loaded.
        fAssertTrue("Profile directory exists", profileDir.exists());

        // TODO: If these tests weren't so expensive to run in automation,
        // this should be two separate tests to avoid storing state between tests.
        testJavaCreatesClientId();
        resetTest(true);
        testJsCreatesClientId();

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
        fAssertFalse("Client id file does not exist yet", getClientIdFile().exists());

        final String clientIdFromJava = getClientIdFromJava();
        final String clientIdFromJS = getClientIdFromJS();
        fAssertEquals("Client ID from Java equals ID from JS", clientIdFromJava, clientIdFromJS);

        final String clientIdFromJavaAgain = getClientIdFromJava();
        final String clientIdFromJSCache = getClientIdFromJS();
        resetJSCache();
        final String clientIdFromJSFileAgain = getClientIdFromJS();
        fAssertEquals("Same client ID retrieved from Java", clientIdFromJava, clientIdFromJavaAgain);
        fAssertEquals("Same client ID retrieved from JS cache", clientIdFromJava, clientIdFromJSCache);
        fAssertEquals("Same client ID retrieved from JS file", clientIdFromJava, clientIdFromJSFileAgain);
    }

    /**
     * Scenario: JS creates client ID
     *   * Fennec starts on a fresh profile
     *   * Js creates the client ID in datareporting/state.json
     *   * Java access the client ID from the same file
     *   * Assert the client IDs are the same
     */
    private void testJsCreatesClientId() throws Exception {
        fAssertFalse("Client id file does not exist yet", getClientIdFile().exists());

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
     * Resets the client ID cache in ClientID.jsm. This method *must* be called after
     * Gecko is loaded or else this method will hang.
     */
    private void resetJSCache() {
        // HACK: the backing JS method is a promise with no return value. Rather than writing a method
        // to handle this (for time reasons), I call the get String method and don't access the return value.
        getBlockingFromJsString("reset");
    }

    private File getClientIdFile() {
        return new File(profileDir, CLIENT_ID_PATH);
    }
}
