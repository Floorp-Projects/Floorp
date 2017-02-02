/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

import org.json.JSONObject;
import org.json.JSONException;

public class testAndroidCastDeviceProvider extends JavascriptTest implements BundleEventListener {
    public testAndroidCastDeviceProvider() {
        super("testAndroidCastDeviceProvider.js");
    }

    @Override // BundleEventListener
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
      mAsserter.dumpLog("Got event: " + event);

      if ("AndroidCastDevice:Start".equals(event)) {
        callback.sendSuccess("Succeed to start presentation.");
        GeckoAppShell.notifyObservers("presentation-view-ready", "chromecast");

      } else if ("AndroidCastDevice:SyncDevice".equals(event)) {
        final JSONObject json = new JSONObject();
        try {
            json.put("uuid", "existed-chromecast");
            json.put("friendlyName", "existed-chromecast");
            json.put("type", "chromecast");
        } catch (JSONException ex) {
        }
        GeckoAppShell.notifyObservers("AndroidCastDevice:Added", json.toString());
      }
    }

    @Override
    public void setUp() throws Exception {
      super.setUp();
      EventDispatcher.getInstance().registerGeckoThreadListener(this, "AndroidCastDevice:Start",
                                                                      "AndroidCastDevice:SyncDevice");
    }

    @Override
    public void tearDown() throws Exception {
      super.tearDown();
      EventDispatcher.getInstance().unregisterGeckoThreadListener(this, "AndroidCastDevice:Start",
                                                                        "AndroidCastDevice:SyncDevice");
    }
}
