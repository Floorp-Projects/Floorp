/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.mdns;

import org.mozilla.gecko.AppConstants.Versions;

import org.mozilla.gecko.util.EventCallback;

import android.annotation.TargetApi;
import android.net.nsd.NsdManager;
import android.net.nsd.NsdServiceInfo;
import android.util.Log;

import java.util.Map;

@TargetApi(16)
class RegistrationListener implements NsdManager.RegistrationListener {
    private static final boolean DEBUG = false;
    private static final String LOGTAG = "GeckoMDNSManager";
    private final NsdManager nsdManager;

    // Callbacks are called from different thread, and every callback can be called only once.
    private EventCallback mStartCallback = null;
    private EventCallback mStopCallback = null;

    RegistrationListener(final NsdManager nsdManager) {
        this.nsdManager = nsdManager;
    }

    public void registerService(final int port, final String serviceName, final String serviceType,
                                final Map<String, String> attributes, final EventCallback callback) {
        if (DEBUG) {
            Log.d(LOGTAG, "registerService: " + serviceName + "." + serviceType + ":" + port);
        }

        NsdServiceInfo serviceInfo = new NsdServiceInfo();
        serviceInfo.setPort(port);
        serviceInfo.setServiceName(serviceName);
        serviceInfo.setServiceType(serviceType);
        setAttributes(serviceInfo, attributes);

        synchronized (this) {
            mStartCallback = callback;
        }
        nsdManager.registerService(serviceInfo, NsdManager.PROTOCOL_DNS_SD, this);
    }

    @TargetApi(21)
    private void setAttributes(final NsdServiceInfo serviceInfo, final Map<String, String> attributes) {
        if (attributes == null || !Versions.feature21Plus) {
            return;
        }

        for (Map.Entry<String, String> entry : attributes.entrySet()) {
            serviceInfo.setAttribute(entry.getKey(), entry.getValue());
        }
    }

    public void unregisterService(final EventCallback callback) {
        if (DEBUG) {
            Log.d(LOGTAG, "unregisterService");
        }
        synchronized (this) {
            mStopCallback = callback;
        }

        nsdManager.unregisterService(this);
    }

    @Override
    public synchronized void onServiceRegistered(final NsdServiceInfo serviceInfo) {
        if (DEBUG) {
            Log.d(LOGTAG, "onServiceRegistered: " + serviceInfo.getServiceName());
        }

        EventCallback callback;
        synchronized (this) {
            callback = mStartCallback;
        }

        if (callback == null) {
            return;
        }

        callback.sendSuccess(NsdMulticastDNSManager.toBundle(serviceInfo));
    }

    @Override
    public synchronized void onRegistrationFailed(final NsdServiceInfo serviceInfo, final int errorCode) {
        Log.e(LOGTAG, "onRegistrationFailed: " + serviceInfo.getServiceName() + "(" + errorCode + ")");

        EventCallback callback;
        synchronized (this) {
            callback = mStartCallback;
        }

        callback.sendError(errorCode);
    }

    @Override
    public synchronized void onServiceUnregistered(final NsdServiceInfo serviceInfo) {
        if (DEBUG) {
            Log.d(LOGTAG, "onServiceUnregistered: " + serviceInfo.getServiceName());
        }

        EventCallback callback;
        synchronized (this) {
            callback = mStopCallback;
        }

        if (callback == null) {
            return;
        }

        callback.sendSuccess(NsdMulticastDNSManager.toBundle(serviceInfo));
    }

    @Override
    public synchronized void onUnregistrationFailed(final NsdServiceInfo serviceInfo, final int errorCode) {
        Log.e(LOGTAG, "onUnregistrationFailed: " + serviceInfo.getServiceName() + "(" + errorCode + ")");

        EventCallback callback;
        synchronized (this) {
            callback = mStopCallback;
        }

        if (callback == null) {
            return;
        }

        callback.sendError(errorCode);
    }
}
