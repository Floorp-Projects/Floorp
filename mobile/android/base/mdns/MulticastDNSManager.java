/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.mdns;

import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoRequest;
import org.mozilla.gecko.util.NativeEventListener;
import org.mozilla.gecko.util.NativeJSObject;
import org.mozilla.gecko.util.ThreadUtils;

import org.json.JSONException;
import org.json.JSONObject;

import android.annotation.TargetApi;
import android.content.Context;
import android.net.nsd.NsdManager;
import android.net.nsd.NsdServiceInfo;
import android.support.annotation.UiThread;
import android.util.Log;

import java.net.InetAddress;
import java.util.HashMap;
import java.util.Map;

/**
 * This class is the bridge between XPCOM mDNS module and NsdManager.
 *
 * @See nsIDNSServiceDiscovery.idl
 */
public abstract class MulticastDNSManager {
    protected static final String LOGTAG = "GeckoMDNSManager";
    private static MulticastDNSManager instance = null;

    public static MulticastDNSManager getInstance(final Context context) {
        if (instance == null) {
            // Bug 1188935: There's a bug on Android 4.4 and before.
            if (Versions.feature21Plus) {
                instance = new NsdMulticastDNSManager(context);
            } else {
                instance = new DummyMulticastDNSManager();
            }
        }
        return instance;
    }

    public abstract void init();
    public abstract void tearDown();
}

/**
 * Mix-in class for MulticastDNSManagers to call EventDispatcher.
 */
class MulticastDNSEventManager {
    private NativeEventListener mListener = null;
    private boolean mEventsRegistered = false;

    MulticastDNSEventManager(NativeEventListener listener) {
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

class NsdMulticastDNSManager extends MulticastDNSManager implements NativeEventListener {
    private final NsdManager nsdManager;
    private final MulticastDNSEventManager mEventManager;
    private DiscoveryListener mDiscoveryListener = null;
    private RegistrationListener mRegistrationListener = null;

    @TargetApi(16)
    public NsdMulticastDNSManager(final Context context) {
        nsdManager = (NsdManager) context.getSystemService(Context.NSD_SERVICE);
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

    @Override
    public void handleMessage(final String event, final NativeJSObject message, final EventCallback callback) {
        Log.v(LOGTAG, "handleMessage: " + event);

        switch (event) {
            case "NsdManager:DiscoverServices":
                if (mDiscoveryListener != null) {
                    mDiscoveryListener.stopServiceDiscovery(null);
                }
                mDiscoveryListener = new DiscoveryListener(nsdManager);
                mDiscoveryListener.discoverServices(message.getString("serviceType"), callback);
                break;
            case "NsdManager:StopServiceDiscovery":
                if (mDiscoveryListener != null) {
                    mDiscoveryListener.stopServiceDiscovery(callback);
                    mDiscoveryListener = null;
                }
                break;
            case "NsdManager:RegisterService":
                if (mRegistrationListener != null) {
                    mRegistrationListener.unregisterService(null);
                }
                mRegistrationListener = new RegistrationListener(nsdManager);
                mRegistrationListener.registerService(message.getInt("port"),
                        message.optString("serviceName", android.os.Build.MODEL),
                        message.getString("serviceType"),
                        parseAttributes(message.optObjectArray("attributes", null)),
                        callback);
                break;
            case "NsdManager:UnregisterService":
                if (mRegistrationListener != null) {
                    mRegistrationListener.unregisterService(callback);
                    mRegistrationListener = null;
                }
                break;
            case "NsdManager:ResolveService":
                (new ResolveListener(nsdManager)).resolveService(message.getString("serviceName"),
                        message.getString("serviceType"),
                        callback);
                break;
        };
    }

    private Map<String, String> parseAttributes(final NativeJSObject[] jsobjs) {
        if (jsobjs == null || jsobjs.length == 0 || !Versions.feature21Plus) {
            return null;
        }

        Map<String, String> attributes = new HashMap<String, String>(jsobjs.length);
        for (NativeJSObject obj : jsobjs) {
            attributes.put(obj.getString("name"), obj.getString("value"));
        }

        return attributes;
    }

    @TargetApi(16)
    public static JSONObject toJSON(final NsdServiceInfo serviceInfo) throws JSONException {
        JSONObject obj = new JSONObject();

        InetAddress host = serviceInfo.getHost();
        if (host != null) {
            obj.put("host", host.getHostName());
        }

        int port = serviceInfo.getPort();
        if (port != 0) {
            obj.put("port", port);
        }

        String serviceName = serviceInfo.getServiceName();
        if (serviceName != null) {
            obj.put("serviceName", serviceName);
        }

        String serviceType = serviceInfo.getServiceType();
        if (serviceType != null) {
            obj.put("serviceType", serviceType);
        }

        return obj;
    }
}

class DummyMulticastDNSManager extends MulticastDNSManager implements NativeEventListener {
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

    @Override
    public void handleMessage(final String event, final NativeJSObject message, final EventCallback callback) {
        Log.v(LOGTAG, "handleMessage: " + event);
        callback.sendError(FAILURE_UNSUPPORTED);
    }
}

@TargetApi(16)
class DiscoveryListener implements NsdManager.DiscoveryListener {
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
            if (mStartCallback != null) {
                throw new RuntimeException("Previous operation is not finished");
            }
            mStartCallback = callback;
        }
        nsdManager.discoverServices(serviceType, NsdManager.PROTOCOL_DNS_SD, this);
    }

    public void stopServiceDiscovery(final EventCallback callback) {
        synchronized (this) {
            if (mStopCallback != null) {
                throw new RuntimeException("Previous operation is not finished");
            }
            mStopCallback = callback;
        }
        nsdManager.stopServiceDiscovery(this);
    }

    @Override
    public synchronized void onDiscoveryStarted(final String serviceType) {
        Log.d(LOGTAG, "onDiscoveryStarted: " + serviceType);
        if (mStartCallback == null) {
            return;
        }
        mStartCallback.sendSuccess(serviceType);
        mStartCallback = null;
    }

    @Override
    public synchronized void onStartDiscoveryFailed(final String serviceType, final int errorCode) {
        Log.e(LOGTAG, "onStartDiscoveryFailed: " + serviceType + "(" + errorCode + ")");
        if (mStartCallback == null) {
            return;
        }
        mStartCallback.sendError(errorCode);
        mStartCallback = null;
    }

    @Override
    public synchronized void onDiscoveryStopped(final String serviceType) {
        Log.d(LOGTAG, "onDiscoveryStopped: " + serviceType);
        if (mStopCallback == null) {
            return;
        }
        mStopCallback.sendSuccess(serviceType);
        mStopCallback = null;
    }

    @Override
    public synchronized void onStopDiscoveryFailed(final String serviceType, final int errorCode) {
        Log.e(LOGTAG, "onStopDiscoveryFailed: " + serviceType + "(" + errorCode + ")");
        if (mStopCallback == null) {
            return;
        }
        mStopCallback.sendError(errorCode);
        mStopCallback = null;
    }

    @Override
    public void onServiceFound(final NsdServiceInfo serviceInfo) {
        Log.d(LOGTAG, "onServiceFound: " + serviceInfo.getServiceName());
        JSONObject json = null;
        try {
            json = NsdMulticastDNSManager.toJSON(serviceInfo);
        } catch (JSONException e) {
            throw new RuntimeException(e);
        }
        GeckoAppShell.sendRequestToGecko(new GeckoRequest("NsdManager:ServiceFound", json) {
            @Override
            public void onResponse(NativeJSObject nativeJSObject) {
                // don't care return value.
            }
        });
    }

    @Override
    public void onServiceLost(final NsdServiceInfo serviceInfo) {
        Log.d(LOGTAG, "onServiceLost: " + serviceInfo.getServiceName());
        JSONObject json = null;
        try {
            json = NsdMulticastDNSManager.toJSON(serviceInfo);
        } catch (JSONException e) {
            throw new RuntimeException(e);
        }
        GeckoAppShell.sendRequestToGecko(new GeckoRequest("NsdManager:ServiceLost", json) {
            @Override
            public void onResponse(NativeJSObject nativeJSObject) {
                // don't care return value.
            }
        });
    }
}

@TargetApi(16)
class RegistrationListener implements NsdManager.RegistrationListener {
    private static final String LOGTAG = "GeckoMDNSManager";
    private final NsdManager nsdManager;

    // Callbacks are called from different thread, and every callback can be called only once.
    private EventCallback mStartCallback = null;
    private EventCallback mStopCallback = null;

    RegistrationListener(final NsdManager nsdManager) {
        this.nsdManager = nsdManager;
    }

    public void registerService(final int port, final String serviceName, final String serviceType, final Map<String, String> attributes, final EventCallback callback) {
        Log.d(LOGTAG, "registerService: " + serviceName + "." + serviceType + ":" + port);
        synchronized (this) {
            if (mStartCallback != null) {
                throw new RuntimeException("Previous operation is not finished");
            }
            mStartCallback = callback;
        }

        NsdServiceInfo serviceInfo = new NsdServiceInfo();
        serviceInfo.setPort(port);
        serviceInfo.setServiceName(serviceName);
        serviceInfo.setServiceType(serviceType);
        setAttributes(serviceInfo, attributes);
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
        Log.d(LOGTAG, "unregisterService");
        synchronized (this) {
            if (mStopCallback != null) {
                throw new RuntimeException("Previous operation is not finished");
            }
            mStopCallback = callback;
        }
        nsdManager.unregisterService(this);
    }

    @Override
    public synchronized void onServiceRegistered(final NsdServiceInfo serviceInfo) {
        Log.d(LOGTAG, "onServiceRegistered: " + serviceInfo.getServiceName());

        if (mStartCallback == null) {
            return;
        }

        try {
            mStartCallback.sendSuccess(NsdMulticastDNSManager.toJSON(serviceInfo));
        } catch (JSONException e) {
            throw new RuntimeException(e);
        }
        mStartCallback = null;
    }

    @Override
    public synchronized void onRegistrationFailed(final NsdServiceInfo serviceInfo, final int errorCode) {
        Log.e(LOGTAG, "onRegistrationFailed: " + serviceInfo.getServiceName() + "(" + errorCode + ")");
        if (mStartCallback == null) {
            return;
        }
        mStartCallback.sendError(errorCode);
        mStartCallback = null;
    }

    @Override
    public synchronized void onServiceUnregistered(final NsdServiceInfo serviceInfo) {
        Log.d(LOGTAG, "onServiceUnregistered: " + serviceInfo.getServiceName());
        if (mStopCallback == null) {
            return;
        }
        try {
            mStopCallback.sendSuccess(NsdMulticastDNSManager.toJSON(serviceInfo));
        } catch (JSONException e) {
            throw new RuntimeException(e);

        }
        mStopCallback = null;
    }

    @Override
    public synchronized void onUnregistrationFailed(final NsdServiceInfo serviceInfo, final int errorCode) {
        Log.e(LOGTAG, "onUnregistrationFailed: " + serviceInfo.getServiceName() + "(" + errorCode + ")");
        if (mStopCallback == null) {
            return;
        }
        mStopCallback.sendError(errorCode);
        mStopCallback = null;
    }
}

@TargetApi(16)
class ResolveListener implements NsdManager.ResolveListener {
    private static final String LOGTAG = "GeckoMDNSManager";
    private final NsdManager nsdManager;

    // Callback is called from different thread, and the callback can be called only once.
    private EventCallback mCallback = null;

    public ResolveListener(final NsdManager nsdManager) {
        this.nsdManager = nsdManager;
    }

    public void resolveService(final String serviceName, final String serviceType, final EventCallback callback) {
        synchronized (this) {
            if (mCallback != null) {
                throw new RuntimeException("Previous operation is not finished");
            }
            mCallback = callback;
        }

        NsdServiceInfo serviceInfo = new NsdServiceInfo();
        serviceInfo.setServiceName(serviceName);
        serviceInfo.setServiceType(serviceType);
        nsdManager.resolveService(serviceInfo, this);
    }


    @Override
    public synchronized void onResolveFailed(final NsdServiceInfo serviceInfo, final int errorCode) {
        Log.e(LOGTAG, "onResolveFailed: " + serviceInfo.getServiceName() + "(" + errorCode + ")");
        if (mCallback == null) {
            return;
        }
        mCallback.sendError(errorCode);
        mCallback = null;
    }

    @Override
    public synchronized void onServiceResolved(final NsdServiceInfo serviceInfo) {
        Log.d(LOGTAG, "onServiceResolved: " + serviceInfo.getServiceName());
        if (mCallback == null) {
            return;
        }

        try {
            mCallback.sendSuccess(NsdMulticastDNSManager.toJSON(serviceInfo));
        } catch (JSONException e) {
            throw new RuntimeException(e);
        }
        mCallback = null;
    }
}
