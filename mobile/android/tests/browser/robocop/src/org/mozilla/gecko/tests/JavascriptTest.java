/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.tests.helpers.JavascriptBridge;
import org.mozilla.gecko.tests.helpers.JavascriptMessageParser;
import org.mozilla.gecko.util.GeckoBundle;

import android.util.Log;

/**
 * Extended to test stand-alone Javascript in automation. If you're looking to test JS interactions
 * with Java, see {@link JavascriptBridgeTest}.
 *
 * There are also other tests that run stand-alone javascript but are more difficult for the mobile
 * team to run (e.g. xpcshell).
 */
public class JavascriptTest extends BaseTest {
    private static final String LOGTAG = "JavascriptTest";
    private static final String EVENT_TYPE = JavascriptBridge.EVENT_TYPE;

    // Calculate these once, at initialization. isLoggable is too expensive to
    // have in-line in each log call.
    private static final boolean logDebug   = Log.isLoggable(LOGTAG, Log.DEBUG);
    private static final boolean logVerbose = Log.isLoggable(LOGTAG, Log.VERBOSE);

    private final String javascriptUrl;

    public JavascriptTest(String javascriptUrl) {
        super();
        this.javascriptUrl = javascriptUrl;
    }

    public void testJavascript() throws Exception {
        blockForGeckoReady();

        doTestJavascript();
    }

    protected void doTestJavascript() throws Exception {
        // We want to be waiting for Robocop messages before the page is loaded
        // because the test harness runs each test in the suite (and possibly
        // completes testing) before the page load event is fired.
        final Actions.EventExpecter expecter =
                mActions.expectGlobalEvent(Actions.EventType.GECKO, EVENT_TYPE);
        mAsserter.dumpLog("Registered listener for " + EVENT_TYPE);

        final String url = getAbsoluteUrl(mStringHelper.getHarnessUrlForJavascript(javascriptUrl));
        mAsserter.dumpLog("Loading JavaScript test from " + url);
        loadUrl(url);

        final JavascriptMessageParser testMessageParser =
                new JavascriptMessageParser(mAsserter, false);
        try {
            while (!testMessageParser.isTestFinished()) {
                if (logVerbose) {
                    Log.v(LOGTAG, "Waiting for " + EVENT_TYPE);
                }
                final GeckoBundle o = expecter.blockForBundle();
                if (logVerbose) {
                    Log.v(LOGTAG, "Got event with data '" + o + "'");
                }

                String innerType = o.getString("innerType");
                if (!"progress".equals(innerType)) {
                    throw new Exception("Unexpected event innerType " + innerType);
                }

                String message = o.getString("message");
                if (message == null) {
                    throw new Exception("Progress message must not be null");
                }
                testMessageParser.logMessage(message);
            }

            if (logDebug) {
                Log.d(LOGTAG, "Got test finished message");
            }
        } finally {
            expecter.unregisterListener();
            mAsserter.dumpLog("Unregistered listener for " + EVENT_TYPE);
        }
    }
}
