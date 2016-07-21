/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.annotation.JNITarget;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.util.NativeEventListener;
import org.mozilla.gecko.util.NativeJSObject;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.NetworkUtils;
import org.mozilla.gecko.util.NetworkUtils.ConnectionSubType;
import org.mozilla.gecko.util.NetworkUtils.ConnectionType;
import org.mozilla.gecko.util.NetworkUtils.NetworkStatus;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.DhcpInfo;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.telephony.TelephonyManager;
import android.text.format.Formatter;
import android.util.Log;

/**
 * Provides connection type, subtype and general network status (up/down).
 *
 * According to spec of Network Information API version 3, connection types include:
 * bluetooth, cellular, ethernet, none, wifi and other. The objective of providing such general
 * connection is due to some security concerns. In short, we don't want to expose exact network type,
 * especially the cellular network type.
 *
 * Specific mobile subtypes are mapped to general 2G, 3G and 4G buckets.
 *
 * Logic is implemented as a state machine, so see the transition matrix to figure out what happens when.
 * This class depends on access to the context, so only use after GeckoAppShell has been initialized.
 */
public class GeckoNetworkManager extends BroadcastReceiver implements NativeEventListener {
    private static final String LOGTAG = "GeckoNetworkManager";

    private static final String LINK_DATA_CHANGED = "changed";

    private static GeckoNetworkManager instance;

    public static void destroy() {
        if (instance != null) {
            instance.onDestroy();
            instance = null;
        }
    }

    public enum ManagerState {
        OffNoListeners,
        OffWithListeners,
        OnNoListeners,
        OnWithListeners
    }

    public enum ManagerEvent {
        start,
        stop,
        enableNotifications,
        disableNotifications,
        receivedUpdate
    }

    private ManagerState currentState = ManagerState.OffNoListeners;
    private ConnectionType currentConnectionType = ConnectionType.NONE;
    private ConnectionType previousConnectionType = ConnectionType.NONE;
    private ConnectionSubType currentConnectionSubtype = ConnectionSubType.UNKNOWN;
    private ConnectionSubType previousConnectionSubtype = ConnectionSubType.UNKNOWN;
    private NetworkStatus currentNetworkStatus = NetworkStatus.UNKNOWN;
    private NetworkStatus previousNetworkStatus = NetworkStatus.UNKNOWN;

    private enum InfoType {
        MCC,
        MNC
    }

    private GeckoNetworkManager() {
        EventDispatcher.getInstance().registerGeckoThreadListener(this,
                "Wifi:Enable",
                "Wifi:GetIPAddress");
    }

    private void onDestroy() {
        handleManagerEvent(ManagerEvent.stop);
        EventDispatcher.getInstance().unregisterGeckoThreadListener(this,
                "Wifi:Enable",
                "Wifi:GetIPAddress");
    }

    public static GeckoNetworkManager getInstance() {
        if (instance == null) {
            instance = new GeckoNetworkManager();
        }

        return instance;
    }

    public double[] getCurrentInformation() {
        final Context applicationContext = GeckoAppShell.getApplicationContext();
        final ConnectionType connectionType = currentConnectionType;
        return new double[] {
                connectionType.value,
                connectionType == ConnectionType.WIFI ? 1.0 : 0.0,
                connectionType == ConnectionType.WIFI ? wifiDhcpGatewayAddress(applicationContext) : 0.0
        };
    }

    @Override
    public void onReceive(Context aContext, Intent aIntent) {
        handleManagerEvent(ManagerEvent.receivedUpdate);
    }

    public void start() {
        handleManagerEvent(ManagerEvent.start);
    }

    public void stop() {
        handleManagerEvent(ManagerEvent.stop);
    }

    public void enableNotifications() {
        handleManagerEvent(ManagerEvent.enableNotifications);
    }

    public void disableNotifications() {
        handleManagerEvent(ManagerEvent.disableNotifications);
    }

    /**
     * For a given event, figure out the next state, run any transition by-product actions, and switch
     * current state to the next state. If event is invalid for the current state, this is a no-op.
     *
     * @param event Incoming event
     * @return Boolean indicating if transition was performed.
     */
    private synchronized boolean handleManagerEvent(ManagerEvent event) {
        final ManagerState nextState = getNextState(currentState, event);

        Log.d(LOGTAG, "Incoming event " + event + " for state " + currentState + " -> " + nextState);
        if (nextState == null) {
            Log.w(LOGTAG, "Invalid event " + event + " for state " + currentState);
            return false;
        }

        performActionsForStateEvent(currentState, event);
        currentState = nextState;

        return true;
    }

    /**
     * Defines a transition matrix for our state machine. For a given state/event pair, returns nextState.
     *
     * @param currentState Current state against which we have an incoming event
     * @param event Incoming event for which we'd like to figure out the next state
     * @return State into which we should transition as result of given event
     */
    @Nullable
    public static ManagerState getNextState(@NonNull ManagerState currentState, @NonNull ManagerEvent event) {
        switch (currentState) {
            case OffNoListeners:
                switch (event) {
                    case start:
                        return ManagerState.OnNoListeners;
                    case enableNotifications:
                        return ManagerState.OffWithListeners;
                    default:
                        return null;
                }
            case OnNoListeners:
                switch (event) {
                    case stop:
                        return ManagerState.OffNoListeners;
                    case enableNotifications:
                        return ManagerState.OnWithListeners;
                    case receivedUpdate:
                        return ManagerState.OnNoListeners;
                    default:
                        return null;
                }
            case OnWithListeners:
                switch (event) {
                    case stop:
                        return ManagerState.OffWithListeners;
                    case disableNotifications:
                        return ManagerState.OnNoListeners;
                    case receivedUpdate:
                        return ManagerState.OnWithListeners;
                    default:
                        return null;
                }
            case OffWithListeners:
                switch (event) {
                    case start:
                        return ManagerState.OnWithListeners;
                    case disableNotifications:
                        return ManagerState.OffNoListeners;
                    default:
                        return null;
                }
            default:
                throw new IllegalStateException("Unknown current state: " + currentState.name());
        }
    }

    /**
     * For a given state/event combination, run any actions which are by-products of leaving the state
     * because of a given event. Since this is a deterministic state machine, we can easily do that
     * without any additional information.
     *
     * @param currentState State which we are leaving
     * @param event Event which is causing us to leave the state
     */
    private void performActionsForStateEvent(ManagerState currentState, ManagerEvent event) {
        // NB: network state might be queried via getCurrentInformation at any time; pre-rewrite behaviour was
        // that network state was updated whenever enableNotifications was called. To avoid deviating
        // from previous behaviour and causing weird side-effects, we call updateNetworkStateAndConnectionType
        // whenever notifications are enabled.
        switch (currentState) {
            case OffNoListeners:
                if (event == ManagerEvent.start) {
                    updateNetworkStateAndConnectionType();
                    registerBroadcastReceiver();
                }
                if (event == ManagerEvent.enableNotifications) {
                    updateNetworkStateAndConnectionType();
                }
                break;
            case OnNoListeners:
                if (event == ManagerEvent.receivedUpdate) {
                    updateNetworkStateAndConnectionType();
                    sendNetworkStateToListeners();
                }
                if (event == ManagerEvent.enableNotifications) {
                    updateNetworkStateAndConnectionType();
                    registerBroadcastReceiver();
                }
                if (event == ManagerEvent.stop) {
                    unregisterBroadcastReceiver();
                }
                break;
            case OnWithListeners:
                if (event == ManagerEvent.receivedUpdate) {
                    updateNetworkStateAndConnectionType();
                    sendNetworkStateToListeners();
                }
                if (event == ManagerEvent.stop) {
                    unregisterBroadcastReceiver();
                }
                /* no-op event: ManagerEvent.disableNotifications */
                break;
            case OffWithListeners:
                if (event == ManagerEvent.start) {
                    registerBroadcastReceiver();
                }
                /* no-op event: ManagerEvent.disableNotifications */
                break;
            default:
                throw new IllegalStateException("Unknown current state: " + currentState.name());
        }
    }

    /**
     * Update current network state and connection types.
     */
    private void updateNetworkStateAndConnectionType() {
        final Context applicationContext = GeckoAppShell.getApplicationContext();
        final ConnectivityManager connectivityManager = (ConnectivityManager) applicationContext.getSystemService(
                Context.CONNECTIVITY_SERVICE);
        // Type/status getters below all have a defined behaviour for when connectivityManager == null
        if (connectivityManager == null) {
            Log.e(LOGTAG, "ConnectivityManager does not exist.");
        }
        currentConnectionType = NetworkUtils.getConnectionType(connectivityManager);
        currentNetworkStatus = NetworkUtils.getNetworkStatus(connectivityManager);
        currentConnectionSubtype = NetworkUtils.getConnectionSubType(connectivityManager);
        Log.d(LOGTAG, "New network state: " + currentNetworkStatus + ", " + currentConnectionType + ", " + currentConnectionSubtype);
    }

    @WrapForJNI
    private static native void onConnectionChanged(int type, String subType,
                                                   boolean isWifi, int DHCPGateway);

    @WrapForJNI
    private static native void onStatusChanged(String status);

    /**
     * Send current network state and connection type as a GeckoEvent, to whomever is listening.
     */
    private void sendNetworkStateToListeners() {
        if (currentConnectionType != previousConnectionType ||
                currentConnectionSubtype != previousConnectionSubtype) {
            previousConnectionType = currentConnectionType;
            previousConnectionSubtype = currentConnectionSubtype;

            final boolean isWifi = currentConnectionType == ConnectionType.WIFI;
            final int gateway = !isWifi ? 0 :
                    wifiDhcpGatewayAddress(GeckoAppShell.getApplicationContext());

            if (GeckoThread.isRunning()) {
                onConnectionChanged(currentConnectionType.value,
                                    currentConnectionSubtype.value, isWifi, gateway);
            } else {
                GeckoThread.queueNativeCall(GeckoNetworkManager.class, "onConnectionChanged",
                                            currentConnectionType.value,
                                            String.class, currentConnectionSubtype.value,
                                            isWifi, gateway);
            }
        }

        final String status;

        if (currentNetworkStatus != previousNetworkStatus) {
            previousNetworkStatus = currentNetworkStatus;
            status = currentNetworkStatus.value;
        } else {
            status = LINK_DATA_CHANGED;
        }

        if (GeckoThread.isRunning()) {
            onStatusChanged(status);
        } else {
            GeckoThread.queueNativeCall(GeckoNetworkManager.class, "onStatusChanged",
                                        String.class, status);
        }
    }

    /**
     * Stop listening for network state updates.
     */
    private void unregisterBroadcastReceiver() {
        GeckoAppShell.getApplicationContext().unregisterReceiver(this);
    }

    /**
     * Start listening for network state updates.
     */
    private void registerBroadcastReceiver() {
        final IntentFilter filter = new IntentFilter(ConnectivityManager.CONNECTIVITY_ACTION);
        GeckoAppShell.getApplicationContext().registerReceiver(this, filter);
    }

    private static int wifiDhcpGatewayAddress(Context context) {
        if (context == null) {
            return 0;
        }

        try {
            WifiManager mgr = (WifiManager) context.getSystemService(Context.WIFI_SERVICE);
            DhcpInfo d = mgr.getDhcpInfo();
            if (d == null) {
                return 0;
            }

            return d.gateway;

        } catch (Exception ex) {
            // getDhcpInfo() is not documented to require any permissions, but on some devices
            // requires android.permission.ACCESS_WIFI_STATE. Just catch the generic exception
            // here and returning 0. Not logging because this could be noisy.
            return 0;
        }
    }

    @Override
    /**
     * Handles native messages, not part of the state machine flow.
     */
    public void handleMessage(final String event, final NativeJSObject message,
                              final EventCallback callback) {
        final Context applicationContext = GeckoAppShell.getApplicationContext();
        switch (event) {
            case "Wifi:Enable":
                final WifiManager mgr = (WifiManager) applicationContext.getSystemService(Context.WIFI_SERVICE);

                if (!mgr.isWifiEnabled()) {
                    mgr.setWifiEnabled(true);
                } else {
                    // If Wifi is enabled, maybe you need to select a network
                    Intent intent = new Intent(android.provider.Settings.ACTION_WIFI_SETTINGS);
                    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    applicationContext.startActivity(intent);
                }
                break;
            case "Wifi:GetIPAddress":
                getWifiIPAddress(callback);
                break;
        }
    }

    // This function only works for IPv4
    private void getWifiIPAddress(final EventCallback callback) {
        final WifiManager mgr = (WifiManager) GeckoAppShell.getApplicationContext().getSystemService(Context.WIFI_SERVICE);

        if (mgr == null) {
            callback.sendError("Cannot get WifiManager");
            return;
        }

        final WifiInfo info = mgr.getConnectionInfo();
        if (info == null) {
            callback.sendError("Cannot get connection info");
            return;
        }

        int ip = info.getIpAddress();
        if (ip == 0) {
            callback.sendError("Cannot get IPv4 address");
            return;
        }
        callback.sendSuccess(Formatter.formatIpAddress(ip));
    }

    private static int getNetworkOperator(InfoType type, Context context) {
        if (null == context) {
            return -1;
        }

        TelephonyManager tel = (TelephonyManager) context.getSystemService(Context.TELEPHONY_SERVICE);
        if (tel == null) {
            Log.e(LOGTAG, "Telephony service does not exist");
            return -1;
        }

        String networkOperator = tel.getNetworkOperator();
        if (networkOperator == null || networkOperator.length() <= 3) {
            return -1;
        }

        if (type == InfoType.MNC) {
            return Integer.parseInt(networkOperator.substring(3));
        }

        if (type == InfoType.MCC) {
            return Integer.parseInt(networkOperator.substring(0, 3));
        }

        return -1;
    }

    /**
     * These are called from JavaScript ctypes. Avoid letting ProGuard delete them.
     *
     * Note that these methods must only be called after GeckoAppShell has been
     * initialized: they depend on access to the context.
     */
    @JNITarget
    public static int getMCC() {
        return getNetworkOperator(InfoType.MCC, GeckoAppShell.getApplicationContext());
    }

    @JNITarget
    public static int getMNC() {
        return getNetworkOperator(InfoType.MNC, GeckoAppShell.getApplicationContext());
    }
}
