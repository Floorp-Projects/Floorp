/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.mdns;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.ThreadUtils;

import android.support.annotation.UiThread;

/**
 * Mix-in class for MulticastDNSManagers to call EventDispatcher.
 */
class MulticastDNSEventManager {
    private BundleEventListener mListener = null;
    private boolean mEventsRegistered = false;

    MulticastDNSEventManager(final BundleEventListener listener) {
        mListener = listener;
    }

    @UiThread
    public void init() {
        ThreadUtils.assertOnUiThread();

        if (mEventsRegistered || mListener == null) {
            return;
        }

        registerEvents();
        mEventsRegistered = true;
    }

    @UiThread
    public void tearDown() {
        ThreadUtils.assertOnUiThread();

        if (!mEventsRegistered || mListener == null) {
            return;
        }

        unregisterEvents();
        mEventsRegistered = false;
    }

    private void registerEvents() {
        EventDispatcher.getInstance().registerGeckoThreadListener(mListener,
                "NsdManager:DiscoverServices",
                "NsdManager:StopServiceDiscovery",
                "NsdManager:RegisterService",
                "NsdManager:UnregisterService",
                "NsdManager:ResolveService");
    }

    private void unregisterEvents() {
        EventDispatcher.getInstance().unregisterGeckoThreadListener(mListener,
                "NsdManager:DiscoverServices",
                "NsdManager:StopServiceDiscovery",
                "NsdManager:RegisterService",
                "NsdManager:UnregisterService",
                "NsdManager:ResolveService");
    }
}