/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.*;

import org.mozilla.gecko.util.GeckoBundle;

/**
 * Tests the proper operation of JavascriptBridge and JavaBridge,
 * which are used by tests for communication between Java and JS.
 */
public class testJavascriptBridge extends JavascriptBridgeTest {

    private static final String TEST_JS = "testJavascriptBridge.js";

    private boolean syncCallReceived;

    public void testJavascriptBridge() {
        blockForReadyAndLoadJS(TEST_JS);
        getJS().syncCall("check_js_int_arg", 1);
    }

    public void checkJavaIntArg(final int int2) {
        // Async call from JS
        fAssertEquals("Integer argument matches", 2, int2);
        getJS().syncCall("check_js_double_arg", 3.0D);
    }

    public void checkJavaDoubleArg(final double double4) {
        // Async call from JS
        fAssertEquals("Double argument matches", 4.0, double4);
        getJS().syncCall("check_js_boolean_arg", false);
    }

    public void checkJavaBooleanArg(final boolean booltrue) {
        // Async call from JS
        fAssertEquals("Boolean argument matches", true, booltrue);
        getJS().syncCall("check_js_string_arg", "foo");
    }

    public void checkJavaStringArg(final String stringbar) {
        // Async call from JS
        fAssertEquals("String argument matches", "bar", stringbar);
        final GeckoBundle bundle = new GeckoBundle();
        bundle.putString("caller", "java");
        getJS().syncCall("check_js_object_arg", bundle);
    }

    public void checkJavaObjectArg(final GeckoBundle obj) {
        // Async call from JS
        fAssertEquals("Object argument matches", "js", obj.getString("caller"));
        getJS().syncCall("check_js_sync_call");
    }

    public void doJSSyncCall() {
        // Sync call from JS
        syncCallReceived = true;
        getJS().asyncCall("respond_to_js_sync_call");
    }

    public void checkJSSyncCallReceived() {
        fAssertTrue("Received sync call before end of test", syncCallReceived);
        // End of test
    }
}
