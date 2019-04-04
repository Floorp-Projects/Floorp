/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.tests;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

import java.io.File;
import java.net.URI;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.fFail;

public class testThemeInstall extends JavascriptTest implements BundleEventListener {
    private EventCallback mFinishCallback;
    private boolean mFinished;

    public testThemeInstall() {
        super("testThemeInstall.js");
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        // Even inside a test environment, accessing mozAddonManager is only possible from this URL.
        mBaseHostnameUrl = "https://example.com:443/tests";

        EventDispatcher.getInstance().registerUiThreadListener(this, "LightweightTheme:Update");
        EventDispatcher.getInstance().registerUiThreadListener(this, "Robocop:WaitOnUI");
    }

    @Override
    public void handleMessage(String event, GeckoBundle message, EventCallback callback) {
        if ("Robocop:WaitOnUI".equals(event)) {
            mFinishCallback = callback;
            checkTestFinished();
            return;
        }

        mAsserter.is(event, "LightweightTheme:Update", "Received LightweightTheme:Update event");
        try {
            GeckoBundle themeData = message.getBundle("data");
            String headerURL = themeData.getString("headerURL");
            mAsserter.ok(headerURL.endsWith("/testImage.png"),
                    "Theme update has the expected headerURL",null);
            mAsserter.ok(headerURL.startsWith("jar:file://"),
                    "headerURL was transformed to jar:file:// URL", null);
        } catch (Exception e) {
            fFail("Event does not contain expected data: " + e.getMessage());
        }

        mFinished = true;
        checkTestFinished();
    }

    private void checkTestFinished() {
        if (mFinished && mFinishCallback != null) {
            mFinishCallback.sendSuccess(null);
        }
    }

    @Override
    public void tearDown() throws Exception {
        super.tearDown();

        EventDispatcher.getInstance().unregisterUiThreadListener(this, "LightweightTheme:Update");
        EventDispatcher.getInstance().unregisterUiThreadListener(this, "Robocop:WaitOnUI");
    }
}
