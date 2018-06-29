/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.mdns;

import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.BundleEventListener;

import android.annotation.TargetApi;
import android.content.Context;
import android.net.nsd.NsdManager;
import android.net.nsd.NsdServiceInfo;
import android.util.Log;

import java.net.InetAddress;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

class NsdMulticastDNSManager extends MulticastDNSManager implements BundleEventListener {
    private final NsdManager nsdManager;
    private final MulticastDNSEventManager mEventManager;
    private Map<String, DiscoveryListener> mDiscoveryListeners = null;
    private Map<String, RegistrationListener> mRegistrationListeners = null;

    @TargetApi(16)
    public NsdMulticastDNSManager(final Context context) {
        nsdManager = (NsdManager) context.getSystemService(Context.NSD_SERVICE);
        mEventManager = new MulticastDNSEventManager(this);
        mDiscoveryListeners = new ConcurrentHashMap<String, DiscoveryListener>();
        mRegistrationListeners = new ConcurrentHashMap<String, RegistrationListener>();
    }

    @Override
    public void init() {
        mEventManager.init();
    }

    @Override
    public void tearDown() {
        mDiscoveryListeners.clear();
        mRegistrationListeners.clear();

        mEventManager.tearDown();
    }

    @Override // BundleEventListener
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
        if (DEBUG) {
            Log.v(LOGTAG, "handleMessage: " + event);
        }

        switch (event) {
            case "NsdManager:DiscoverServices": {
                DiscoveryListener listener = new DiscoveryListener(nsdManager);
                listener.discoverServices(message.getString("serviceType"), callback);
                mDiscoveryListeners.put(message.getString("uniqueId"), listener);
                break;
            }
            case "NsdManager:StopServiceDiscovery": {
                String uuid = message.getString("uniqueId");
                DiscoveryListener listener = mDiscoveryListeners.remove(uuid);
                if (listener == null) {
                    Log.e(LOGTAG, "DiscoveryListener " + uuid + " was not found.");
                    return;
                }
                listener.stopServiceDiscovery(callback);
                break;
            }
            case "NsdManager:RegisterService": {
                RegistrationListener listener = new RegistrationListener(nsdManager);
                listener.registerService(message.getInt("port"),
                        message.getString("serviceName", android.os.Build.MODEL),
                        message.getString("serviceType"),
                        parseAttributes(message.getBundleArray("attributes")),
                        callback);
                mRegistrationListeners.put(message.getString("uniqueId"), listener);
                break;
            }
            case "NsdManager:UnregisterService": {
                String uuid = message.getString("uniqueId");
                RegistrationListener listener = mRegistrationListeners.remove(uuid);
                if (listener == null) {
                    Log.e(LOGTAG, "RegistrationListener " + uuid + " was not found.");
                    return;
                }
                listener.unregisterService(callback);
                break;
            }
            case "NsdManager:ResolveService": {
                (new ResolveListener(nsdManager)).resolveService(message.getString("serviceName"),
                        message.getString("serviceType"),
                        callback);
                break;
            }
        }
    }

    private Map<String, String> parseAttributes(final GeckoBundle[] jsobjs) {
        if (jsobjs == null || jsobjs.length == 0 || !Versions.feature21Plus) {
            return null;
        }

        final Map<String, String> attributes = new HashMap<>(jsobjs.length);
        for (final GeckoBundle obj : jsobjs) {
            attributes.put(obj.getString("name"), obj.getString("value"));
        }

        return attributes;
    }

    @TargetApi(16)
    public static GeckoBundle toBundle(final NsdServiceInfo serviceInfo) {
        final GeckoBundle obj = new GeckoBundle();

        final InetAddress host = serviceInfo.getHost();
        if (host != null) {
            obj.putString("host", host.getCanonicalHostName());
            obj.putString("address", host.getHostAddress());
        }

        int port = serviceInfo.getPort();
        if (port != 0) {
            obj.putInt("port", port);
        }

        final String serviceName = serviceInfo.getServiceName();
        if (serviceName != null) {
            obj.putString("serviceName", serviceName);
        }

        final String serviceType = serviceInfo.getServiceType();
        if (serviceType != null) {
            obj.putString("serviceType", serviceType);
        }

        return obj;
    }
}

