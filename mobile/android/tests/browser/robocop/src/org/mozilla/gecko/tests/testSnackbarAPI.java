/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.fFail;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

public class testSnackbarAPI extends JavascriptTest implements BundleEventListener {
    // Snackbar.LENGTH_INDEFINITE: To avoid tests depending on the android design support library
    private static final int  SNACKBAR_LENGTH_INDEFINITE = -2;

    public testSnackbarAPI() {
        super("testSnackbarAPI.js");
    }

    @Override
    public void handleMessage(String event, GeckoBundle message, EventCallback callback) {
        if ("Robocop:WaitOnUI".equals(event)) {
            callback.sendSuccess(null);
            return;
        }

        mAsserter.is(event, "Snackbar:Show", "Received Snackbar:Show event");

        try {
            mAsserter.is(message.getString("message"), "This is a Snackbar", "Snackbar message");
            mAsserter.is(message.getInt("duration"), SNACKBAR_LENGTH_INDEFINITE, "Snackbar duration");

            GeckoBundle action = message.getBundle("action");

            mAsserter.is(action.getString("label"), "Click me", "Snackbar action label");

        } catch (Exception e) {
            fFail("Event does not contain expected data: " + e.getMessage());
        }
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        EventDispatcher.getInstance().registerUiThreadListener(this, "Snackbar:Show");
        EventDispatcher.getInstance().registerUiThreadListener(this, "Robocop:WaitOnUI");
    }

    @Override
    public void tearDown() throws Exception {
        super.tearDown();

        EventDispatcher.getInstance().unregisterUiThreadListener(this, "Snackbar:Show");
        EventDispatcher.getInstance().unregisterUiThreadListener(this, "Robocop:WaitOnUI");
    }
}
