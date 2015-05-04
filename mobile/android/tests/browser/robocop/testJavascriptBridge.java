/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.*;

import org.mozilla.gecko.tests.helpers.*;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * Tests the proper operation of JavascriptBridge and JavaBridge,
 * which are used by tests for communication between Java and JS.
 */
public class testJavascriptBridge extends UITest {

    private static final String TEST_JS = "testJavascriptBridge.js";

    private JavascriptBridge js;
    private boolean syncCallReceived;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        js = new JavascriptBridge(this);
    }

    @Override
    public void tearDown() throws Exception {
        js.disconnect();
        super.tearDown();
    }

    public void testJavascriptBridge() {
        GeckoHelper.blockForReady();
        NavigationHelper.enterAndLoadUrl(mStringHelper.getHarnessUrlForJavascript(TEST_JS));
        js.syncCall("check_js_int_arg", 1);
    }

    public void checkJavaIntArg(final int int2) {
        // Async call from JS
        fAssertEquals("Integer argument matches", 2, int2);
        js.syncCall("check_js_double_arg", 3.0D);
    }

    public void checkJavaDoubleArg(final double double4) {
        // Async call from JS
        fAssertEquals("Double argument matches", 4.0, double4);
        js.syncCall("check_js_boolean_arg", false);
    }

    public void checkJavaBooleanArg(final boolean booltrue) {
        // Async call from JS
        fAssertEquals("Boolean argument matches", true, booltrue);
        js.syncCall("check_js_string_arg", "foo");
    }

    public void checkJavaStringArg(final String stringbar) throws JSONException {
        // Async call from JS
        fAssertEquals("String argument matches", "bar", stringbar);
        final JSONObject obj = new JSONObject();
        obj.put("caller", "java");
        js.syncCall("check_js_object_arg", (JSONObject) obj);
    }

    public void checkJavaObjectArg(final JSONObject obj) throws JSONException {
        // Async call from JS
        fAssertEquals("Object argument matches", "js", obj.getString("caller"));
        js.syncCall("check_js_sync_call");
    }

    public void doJSSyncCall() {
        // Sync call from JS
        syncCallReceived = true;
        js.asyncCall("respond_to_js_sync_call");
    }

    public void checkJSSyncCallReceived() {
        fAssertTrue("Received sync call before end of test", syncCallReceived);
        // End of test
    }
}
