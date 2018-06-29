/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.mdns;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.EventCallback;

import android.annotation.TargetApi;
import android.net.nsd.NsdManager;
import android.net.nsd.NsdServiceInfo;
import android.util.Log;

@TargetApi(16)
class DiscoveryListener implements NsdManager.DiscoveryListener {
    private static final boolean DEBUG = false;
    private static final String LOGTAG = "GeckoMDNSManager";
    private final NsdManager nsdManager;

    // Callbacks are called from different thread, and every callback can be called only once.
    private EventCallback mStartCallback = null;
    private EventCallback mStopCallback = null;

    DiscoveryListener(final NsdManager nsdManager) {
        this.nsdManager = nsdManager;
    }

    public void discoverServices(final String serviceType, final EventCallback callback) {
        synchronized (this) {
            mStartCallback = callback;
        }
        nsdManager.discoverServices(serviceType, NsdManager.PROTOCOL_DNS_SD, this);
    }

    public void stopServiceDiscovery(final EventCallback callback) {
        synchronized (this) {
            mStopCallback = callback;
        }
        nsdManager.stopServiceDiscovery(this);
    }

    @Override
    public synchronized void onDiscoveryStarted(final String serviceType) {
        if (DEBUG) {
            Log.d(LOGTAG, "onDiscoveryStarted: " + serviceType);
        }

        EventCallback callback;
        synchronized (this) {
            callback = mStartCallback;
        }

        if (callback == null) {
            return;
        }

        callback.sendSuccess(serviceType);
    }

    @Override
    public synchronized void onStartDiscoveryFailed(final String serviceType, final int errorCode) {
        Log.e(LOGTAG, "onStartDiscoveryFailed: " + serviceType + "(" + errorCode + ")");

        EventCallback callback;
        synchronized (this) {
            callback = mStartCallback;
        }

        callback.sendError(errorCode);
    }

    @Override
    public synchronized void onDiscoveryStopped(final String serviceType) {
        if (DEBUG) {
            Log.d(LOGTAG, "onDiscoveryStopped: " + serviceType);
        }

        EventCallback callback;
        synchronized (this) {
            callback = mStopCallback;
        }

        if (callback == null) {
            return;
        }

        callback.sendSuccess(serviceType);
    }

    @Override
    public synchronized void onStopDiscoveryFailed(final String serviceType, final int errorCode) {
        Log.e(LOGTAG, "onStopDiscoveryFailed: " + serviceType + "(" + errorCode + ")");

        EventCallback callback;
        synchronized (this) {
            callback = mStopCallback;
        }

        if (callback == null) {
            return;
        }

        callback.sendError(errorCode);
    }

    @Override
    public void onServiceFound(final NsdServiceInfo serviceInfo) {
        if (DEBUG) {
            Log.d(LOGTAG, "onServiceFound: " + serviceInfo.getServiceName());
        }

        EventDispatcher.getInstance().dispatch(
                "NsdManager:ServiceFound", NsdMulticastDNSManager.toBundle(serviceInfo));
    }

    @Override
    public void onServiceLost(final NsdServiceInfo serviceInfo) {
        if (DEBUG) {
            Log.d(LOGTAG, "onServiceLost: " + serviceInfo.getServiceName());
        }

        EventDispatcher.getInstance().dispatch(
                "NdManager:ServiceLost", NsdMulticastDNSManager.toBundle(serviceInfo));
    }
}
