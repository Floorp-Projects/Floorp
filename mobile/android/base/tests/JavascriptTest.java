package org.mozilla.gecko.tests;

import org.mozilla.gecko.*;
import org.mozilla.gecko.tests.helpers.JavascriptMessageParser;

import android.util.Log;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;


public class JavascriptTest extends BaseTest {
    public static final String LOGTAG = "JavascriptTest";

    public final String javascriptUrl;

    public JavascriptTest(String javascriptUrl) {
        super();
        this.javascriptUrl = javascriptUrl;
    }

    @Override
    protected int getTestType() {
        return TEST_MOCHITEST;
    }

    public void testJavascript() throws Exception {
        blockForGeckoReady();

        // We want to be waiting for Robocop messages before the page is loaded
        // because the test harness runs each test in the suite (and possibly
        // completes testing) before the page load event is fired.
        final Actions.EventExpecter expecter = mActions.expectGeckoEvent("Robocop:Status");
        mAsserter.dumpLog("Registered listener for Robocop:Status");

        final String url = getAbsoluteUrl("/robocop/robocop_javascript.html?path=" + javascriptUrl);
        mAsserter.dumpLog("Loading JavaScript test from " + url);

        loadUrl(url);

        final JavascriptMessageParser testMessageParser = new JavascriptMessageParser(mAsserter);
        try {
            while (!testMessageParser.isTestFinished()) {
                if (Log.isLoggable(LOGTAG, Log.VERBOSE)) {
                    Log.v(LOGTAG, "Waiting for Robocop:Status");
                }
                String data = expecter.blockForEventData();
                if (Log.isLoggable(LOGTAG, Log.VERBOSE)) {
                    Log.v(LOGTAG, "Got Robocop:Status with data '" + data + "'");
                }

                JSONObject o = new JSONObject(data);
                String innerType = o.getString("innerType");

                if (!"progress".equals(innerType)) {
                    throw new Exception("Unexpected Robocop:Status innerType " + innerType);
                }

                String message = o.getString("message");
                if (message == null) {
                    throw new Exception("Robocop:Status progress message must not be null");
                }

                testMessageParser.logMessage(message);
            }

            if (Log.isLoggable(LOGTAG, Log.DEBUG)) {
                Log.d(LOGTAG, "Got test finished message");
            }
        } finally {
            expecter.unregisterListener();
            mAsserter.dumpLog("Unregistered listener for Robocop:Status");
        }
    }
}
