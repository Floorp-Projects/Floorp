/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.fFail;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

public class testFilePicker extends JavascriptTest implements BundleEventListener {
    private static final String TEST_FILENAME = "/mnt/sdcard/my-favorite-martian.png";

    public testFilePicker() {
        super("testFilePicker.js");
    }

    @Override // BundleEventListener
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
        // We handle the FilePicker message here so we can send back hard coded file information. We
        // don't want to try to emulate "picking" a file using the Android intent chooser.
        if ("FilePicker:Show".equals(event)) {
            callback.sendSuccess(TEST_FILENAME);
        }
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        EventDispatcher.getInstance().registerUiThreadListener(this, "FilePicker:Show");
    }

    @Override
    public void tearDown() throws Exception {
        super.tearDown();

        EventDispatcher.getInstance().unregisterUiThreadListener(this, "FilePicker:Show");
    }
}
