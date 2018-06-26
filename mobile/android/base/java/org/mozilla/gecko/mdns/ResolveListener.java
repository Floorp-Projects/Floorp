/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.mdns;

import org.mozilla.gecko.util.EventCallback;

import android.annotation.TargetApi;
import android.net.nsd.NsdManager;
import android.net.nsd.NsdServiceInfo;
import android.util.Log;

@TargetApi(16)
class ResolveListener implements NsdManager.ResolveListener {
    private static final boolean DEBUG = false;
    private static final String LOGTAG = "GeckoMDNSManager";
    private final NsdManager nsdManager;

    // Callback is called from different thread, and the callback can be called only once.
    private EventCallback mCallback = null;

    public ResolveListener(final NsdManager nsdManager) {
        this.nsdManager = nsdManager;
    }

    public void resolveService(final String serviceName, final String serviceType, final EventCallback callback) {
        NsdServiceInfo serviceInfo = new NsdServiceInfo();
        serviceInfo.setServiceName(serviceName);
        serviceInfo.setServiceType(serviceType);

        mCallback = callback;
        nsdManager.resolveService(serviceInfo, this);
    }


    @Override
    public synchronized void onResolveFailed(final NsdServiceInfo serviceInfo, final int errorCode) {
        Log.e(LOGTAG, "onResolveFailed: " + serviceInfo.getServiceName() + "(" + errorCode + ")");

        if (mCallback == null) {
            return;
        }
        mCallback.sendError(errorCode);
    }

    @Override
    public synchronized void onServiceResolved(final NsdServiceInfo serviceInfo) {
        if (DEBUG) {
            Log.d(LOGTAG, "onServiceResolved: " + serviceInfo.getServiceName());
        }

        if (mCallback == null) {
            return;
        }

        mCallback.sendSuccess(NsdMulticastDNSManager.toBundle(serviceInfo));
    }
}
