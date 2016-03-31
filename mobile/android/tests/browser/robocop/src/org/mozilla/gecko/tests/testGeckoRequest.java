/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import java.util.concurrent.atomic.AtomicBoolean;

import com.robotium.solo.Condition;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.tests.helpers.AssertionHelper;
import org.mozilla.gecko.tests.helpers.WaitHelper;
import org.mozilla.gecko.util.GeckoRequest;
import org.mozilla.gecko.util.NativeJSObject;

/**
 * Tests sending and receiving Gecko requests using the GeckoRequest API.
 */
public class testGeckoRequest extends JavascriptBridgeTest {
    private static final String TEST_JS = "testGeckoRequest.js";
    private static final String REQUEST_EVENT = "Robocop:GeckoRequest";
    private static final String REQUEST_EXCEPTION_EVENT = "Robocop:GeckoRequestException";
    private static final int MAX_WAIT_MS = 5000;

    public void testGeckoRequest() {
        blockForReadyAndLoadJS(TEST_JS);

        // Register a listener for this request.
        getJS().syncCall("add_request_listener", REQUEST_EVENT);

        // Make sure we receive the expected response.
        checkFooRequest();

        // Try registering a second listener for this request, which should fail.
        getJS().syncCall("add_second_request_listener", REQUEST_EVENT);

        // Unregister the listener for this request.
        getJS().syncCall("remove_request_listener", REQUEST_EVENT);

        // Make sure we don't receive a response after removing the listener.
        checkUnregisteredRequest();

        // Check that we still receive a response for listeners that throw.
        getJS().syncCall("add_exception_listener", REQUEST_EXCEPTION_EVENT);
        checkExceptionRequest();
        getJS().syncCall("remove_request_listener", REQUEST_EXCEPTION_EVENT);

        getJS().syncCall("finish_test");
    }

    private void checkFooRequest() {
        final AtomicBoolean responseReceived = new AtomicBoolean(false);
        final String data = "foo";

        GeckoAppShell.sendRequestToGecko(new GeckoRequest(REQUEST_EVENT, data) {
            @Override
            public void onResponse(NativeJSObject nativeJSObject) {
                // Ensure we receive the expected response from Gecko.
                final String result = nativeJSObject.getString("result");
                AssertionHelper.fAssertEquals("Sent and received request data", data + "bar", result);
                responseReceived.set(true);
            }
        });

        WaitHelper.waitFor("Received response for registered listener", new Condition() {
            @Override
            public boolean isSatisfied() {
                return responseReceived.get();
            }
        }, MAX_WAIT_MS);
    }

    private void checkExceptionRequest() {
        final AtomicBoolean responseReceived = new AtomicBoolean(false);
        final AtomicBoolean errorReceived = new AtomicBoolean(false);

        GeckoAppShell.sendRequestToGecko(new GeckoRequest(REQUEST_EXCEPTION_EVENT, null) {
            @Override
            public void onResponse(NativeJSObject nativeJSObject) {
                responseReceived.set(true);
            }

            @Override
            public void onError(NativeJSObject error) {
                errorReceived.set(true);
            }
        });

        WaitHelper.waitFor("Received error for listener with exception", new Condition() {
            @Override
            public boolean isSatisfied() {
                return errorReceived.get();
            }
        }, MAX_WAIT_MS);

        AssertionHelper.fAssertTrue("onResponse not called for listener with exception", !responseReceived.get());
    }

    private void checkUnregisteredRequest() {
        final AtomicBoolean responseReceived = new AtomicBoolean(false);

        GeckoAppShell.sendRequestToGecko(new GeckoRequest(REQUEST_EVENT, null) {
            @Override
            public void onResponse(NativeJSObject nativeJSObject) {
                responseReceived.set(true);
            }
        });

        // This check makes sure that we do *not* receive a response for an unregistered listener,
        // meaning waitForCondition() should always time out.
        getSolo().waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                return responseReceived.get();
            }
        }, MAX_WAIT_MS);

        AssertionHelper.fAssertTrue("Did not receive response for unregistered listener", !responseReceived.get());
    }
}
