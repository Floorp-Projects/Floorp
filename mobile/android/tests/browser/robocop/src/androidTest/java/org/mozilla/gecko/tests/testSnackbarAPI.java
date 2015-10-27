/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.fFail;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.NativeEventListener;
import org.mozilla.gecko.util.NativeJSObject;

public class testSnackbarAPI extends JavascriptTest implements NativeEventListener {
    // Snackbar.LENGTH_INDEFINITE: To avoid tests depending on the android design support library
    private static final int  SNACKBAR_LENGTH_INDEFINITE = -2;

    public testSnackbarAPI() {
        super("testSnackbarAPI.js");
    }

    @Override
    public void handleMessage(String event, NativeJSObject message, EventCallback callback) {
        mAsserter.is(event, "Snackbar:Show", "Received Snackbar:Show event");

        try {
            mAsserter.is(message.getString("message"), "This is a Snackbar", "Snackbar message");
            mAsserter.is(message.getInt("duration"), SNACKBAR_LENGTH_INDEFINITE, "Snackbar duration");

            NativeJSObject action = message.getObject("action");

            mAsserter.is(action.getString("label"), "Click me", "Snackbar action label");

        } catch (Exception e) {
            fFail("Event does not contain expected data: " + e.getMessage());
        }
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        EventDispatcher.getInstance().registerGeckoThreadListener(this, "Snackbar:Show");
    }

    @Override
    public void tearDown() throws Exception {
        super.tearDown();

        EventDispatcher.getInstance().unregisterGeckoThreadListener(this, "Snackbar:Show");
    }
}
