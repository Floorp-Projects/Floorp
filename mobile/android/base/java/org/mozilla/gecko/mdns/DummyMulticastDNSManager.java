/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.mdns;

import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.BundleEventListener;

import android.util.Log;

class DummyMulticastDNSManager extends MulticastDNSManager implements BundleEventListener {
    static final int FAILURE_UNSUPPORTED = -65544;
    private final MulticastDNSEventManager mEventManager;

    public DummyMulticastDNSManager() {
        mEventManager = new MulticastDNSEventManager(this);
    }

    @Override
    public void init() {
        mEventManager.init();
    }

    @Override
    public void tearDown() {
        mEventManager.tearDown();
    }

    @Override // BundleEventListener
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
        if (DEBUG) {
            Log.v(LOGTAG, "handleMessage: " + event);
        }
        callback.sendError(FAILURE_UNSUPPORTED);
    }
}
