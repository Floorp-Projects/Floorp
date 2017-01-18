/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.fFail;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

public class testTrackingProtection extends JavascriptTest implements BundleEventListener {
    private String mLastTracking;

    public testTrackingProtection() {
        super("testTrackingProtection.js");
    }

    @Override // BundleEventListener
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
        if ("Content:SecurityChange".equals(event)) {
            final GeckoBundle identity = message.getBundle("identity");
            final GeckoBundle mode = identity.getBundle("mode");
            mLastTracking = mode.getString("tracking");
            mAsserter.dumpLog("Security change (tracking): " + mLastTracking);

        } else if ("Test:Expected".equals(event)) {
            final String expected = message.getString("expected");
            mAsserter.is(mLastTracking, expected, "Tracking matched expectation");
            mAsserter.dumpLog("Testing (tracking): " + mLastTracking + " = " + expected);
        }
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        EventDispatcher.getInstance().registerUiThreadListener(this,
                                                               "Content:SecurityChange",
                                                               "Test:Expected");
    }

    @Override
    public void tearDown() throws Exception {
        super.tearDown();

        EventDispatcher.getInstance().unregisterUiThreadListener(this,
                                                                 "Content:SecurityChange",
                                                                 "Test:Expected");
    }
}
