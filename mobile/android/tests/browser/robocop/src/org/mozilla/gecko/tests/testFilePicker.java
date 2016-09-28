/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.fFail;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.util.GeckoEventListener;

import org.json.JSONException;
import org.json.JSONObject;

public class testFilePicker extends JavascriptTest implements GeckoEventListener {
    private static final String TEST_FILENAME = "/mnt/sdcard/my-favorite-martian.png";

    public testFilePicker() {
        super("testFilePicker.js");
    }

    @Override
    public void handleMessage(String event, final JSONObject message) {
        // We handle the FilePicker message here so we can send back hard coded file information. We
        // don't want to try to emulate "picking" a file using the Android intent chooser.
        if (event.equals("FilePicker:Show")) {
            try {
                message.put("file", TEST_FILENAME);
            } catch (JSONException ex) {
                fFail("Can't add filename to message " + TEST_FILENAME);
            }

            mActions.sendGeckoEvent("FilePicker:Result", message.toString());
        }
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        GeckoApp.getEventDispatcher().registerGeckoThreadListener(this, "FilePicker:Show");
    }

    @Override
    public void tearDown() throws Exception {
        super.tearDown();

        GeckoApp.getEventDispatcher().unregisterGeckoThreadListener(this, "FilePicker:Show");
    }
}
