/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.NativeEventListener;
import org.mozilla.gecko.util.NativeJSObject;
import org.mozilla.gecko.util.ThreadUtils;

import org.json.JSONArray;

import android.content.Context;
import android.os.Build;
import android.support.annotation.UiThread;
import android.util.Log;

import java.net.InetAddress;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.Set;

public class NetworkInfoService implements NativeEventListener {
    static final String LOGTAG = "NetworkInfoService";
    static NetworkInfoService instance = null;

    static final String MSG_LIST_NETWORK_ADDRESSES = "NetworkInfoService:ListNetworkAddresses";
    static final String MSG_GET_HOSTNAME = "NetworkInfoService:GetHostname";

    public static NetworkInfoService getInstance(final Context context) {
        if (instance == null) {
            instance = new NetworkInfoService(context);
        }
        return instance;
    }

    NetworkInfoService(final Context context) {
    }

    @UiThread
    public void init() {
        ThreadUtils.assertOnUiThread();
        EventDispatcher.getInstance().registerGeckoThreadListener(this,
                MSG_LIST_NETWORK_ADDRESSES,
                MSG_GET_HOSTNAME);
    }

    @UiThread
    public void tearDown() {
        ThreadUtils.assertOnUiThread();
        EventDispatcher.getInstance().unregisterGeckoThreadListener(this,
                MSG_LIST_NETWORK_ADDRESSES,
                MSG_GET_HOSTNAME);
    }

    @Override
    public void handleMessage(final String event, final NativeJSObject message,
                              final EventCallback callback)
    {
        Log.v(LOGTAG, "handleMessage: " + event);

        switch (event) {
            case MSG_LIST_NETWORK_ADDRESSES: {
                handleListNetworkAddresses(callback);
                break;
            }
            case MSG_GET_HOSTNAME: {
                handleGetHostname(callback);
                break;
            }
        }
    }

    void handleListNetworkAddresses(final EventCallback callback) {
        Set<String> addresses = new HashSet<String>();
        try {
            Enumeration<NetworkInterface> ifaceEnum = NetworkInterface.getNetworkInterfaces();
            if (ifaceEnum != null) {
                while (ifaceEnum.hasMoreElements()) {
                    NetworkInterface iface = ifaceEnum.nextElement();
                    Enumeration<InetAddress> addrList = iface.getInetAddresses();
                    while (addrList.hasMoreElements()) {
                        InetAddress addr = addrList.nextElement();
                        addresses.add(addr.getHostAddress());
                    }
                }
            }
        } catch (SocketException exc) {
            callback.sendError(-1);
            return;
        }

        JSONArray array = new JSONArray();
        for (String addr : addresses) {
            array.put(addr);
        }
        callback.sendSuccess(array);
    }

    void handleGetHostname(final EventCallback callback) {
        // callback.sendError(-1);
        callback.sendSuccess(getDeviceName());
    }

    private static String getDeviceName() {
        String manufacturer = Build.MANUFACTURER;
        String model = Build.MODEL;
        if (model.startsWith(manufacturer)) {
            return capitalize(model);
        }
        return capitalize(manufacturer) + " " + model;
    }

    private static String capitalize(String str) {
        if (str.length() <= 1)
            return str;

        // Capitalize the manufacturer's first letter.
        char ch0 = str.charAt(0);
        if (Character.isLetter(ch0) && Character.isLowerCase(ch0)) {
            boolean upcase = true;
            // But don't capitalize the first letter if it's an 'i' followed
            // by a non-lowercase letter.  Sheesh.
            if (ch0 == 'i') {
                if (str.length() >= 2) {
                    char ch1 = str.charAt(1);
                    if (!Character.isLetter(ch1) || !Character.isLowerCase(ch1)) {
                        upcase = false;
                    }
                }
            }
            if (upcase) {
                return Character.toUpperCase(ch0) + str.substring(1);
            }
        }
        return str;
    }

}
