/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.NetworkUtils;
import org.mozilla.gecko.util.NetworkUtils.ConnectionSubType;
import org.mozilla.gecko.util.NetworkUtils.ConnectionType;
import org.mozilla.gecko.util.NetworkUtils.NetworkStatus;

import android.annotation.SuppressLint;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.DhcpInfo;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
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
public class GeckoNetworkManager extends BroadcastReceiver implements BundleEventListener {
    private static final String LOGTAG = "GeckoNetworkManager";

    // If network configuration and/or status changed, we send details of what changed.
    // If we received a "check out new network state!" intent from the OS but nothing in it looks
    // different, we ignore it. See Bug 1330836 for some relevant details.
    private static final String LINK_DATA_CHANGED = "changed";

    private static GeckoNetworkManager instance;

    // We hackishly (yet harmlessly, in this case) keep a Context reference passed in via the start method.
    // See context handling notes in handleManagerEvent, and Bug 1277333.
    private Context mContext;

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

    private ManagerState mCurrentState = ManagerState.OffNoListeners;
    private ConnectionType mCurrentConnectionType = ConnectionType.NONE;
    private ConnectionType mPreviousConnectionType = ConnectionType.NONE;
    private ConnectionSubType mCurrentConnectionSubtype = ConnectionSubType.UNKNOWN;
    private ConnectionSubType mPreviousConnectionSubtype = ConnectionSubType.UNKNOWN;
    private NetworkStatus mCurrentNetworkStatus = NetworkStatus.UNKNOWN;
    private NetworkStatus mPreviousNetworkStatus = NetworkStatus.UNKNOWN;

    private enum InfoType {
        MCC,
        MNC
    }

    private GeckoNetworkManager() {
        EventDispatcher.getInstance().registerUiThreadListener(this,
                "Wifi:GetIPAddress");
    }

    private void onDestroy() {
        handleManagerEvent(ManagerEvent.stop);
        EventDispatcher.getInstance().unregisterUiThreadListener(this,
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
        final ConnectionType connectionType = mCurrentConnectionType;
        return new double[] {
            connectionType.value,
            connectionType == ConnectionType.WIFI ? 1.0 : 0.0,
            connectionType == ConnectionType.WIFI ? wifiDhcpGatewayAddress(applicationContext) : 0.0
        };
    }

    @Override
    public void onReceive(final Context aContext, final Intent aIntent) {
        handleManagerEvent(ManagerEvent.receivedUpdate);
    }

    public void start(final Context context) {
        mContext = context;
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
    private synchronized boolean handleManagerEvent(final ManagerEvent event) {
        final ManagerState nextState = getNextState(mCurrentState, event);

        Log.d(LOGTAG, "Incoming event " + event + " for state " + mCurrentState + " -> " + nextState);
        if (nextState == null) {
            Log.w(LOGTAG, "Invalid event " + event + " for state " + mCurrentState);
            return false;
        }

        // We're being deliberately careful about handling context here; it's possible that in some
        // rare cases and possibly related to timing of when this is called (seems to be early in the startup phase),
        // GeckoAppShell.getApplicationContext() will be null, and .start() wasn't called yet,
        // so we don't have a local Context reference either. If both of these are true, we have to drop the event.
        // NB: this is hacky (and these checks attempt to isolate the hackiness), and root cause
        // seems to be how this class fits into the larger ecosystem and general flow of events.
        // See Bug 1277333.
        final Context contextForAction;
        if (mContext != null) {
            contextForAction = mContext;
        } else {
            contextForAction = GeckoAppShell.getApplicationContext();
        }

        if (contextForAction == null) {
            Log.w(LOGTAG, "Context is not available while processing event " + event + " for state " + mCurrentState);
            return false;
        }

        performActionsForStateEvent(contextForAction, mCurrentState, event);
        mCurrentState = nextState;

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
    public static ManagerState getNextState(final @NonNull ManagerState currentState,
                                            final @NonNull ManagerEvent event) {
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
    private void performActionsForStateEvent(final Context context, final ManagerState currentState, final ManagerEvent event) {
        // NB: network state might be queried via getCurrentInformation at any time; pre-rewrite behaviour was
        // that network state was updated whenever enableNotifications was called. To avoid deviating
        // from previous behaviour and causing weird side-effects, we call updateNetworkStateAndConnectionType
        // whenever notifications are enabled.
        switch (currentState) {
            case OffNoListeners:
                if (event == ManagerEvent.start) {
                    updateNetworkStateAndConnectionType(context);
                    registerBroadcastReceiver(context, this);
                }
                if (event == ManagerEvent.enableNotifications) {
                    updateNetworkStateAndConnectionType(context);
                }
                break;
            case OnNoListeners:
                if (event == ManagerEvent.receivedUpdate) {
                    updateNetworkStateAndConnectionType(context);
                    sendNetworkStateToListeners(context);
                }
                if (event == ManagerEvent.enableNotifications) {
                    updateNetworkStateAndConnectionType(context);
                    registerBroadcastReceiver(context, this);
                }
                if (event == ManagerEvent.stop) {
                    unregisterBroadcastReceiver(context, this);
                }
                break;
            case OnWithListeners:
                if (event == ManagerEvent.receivedUpdate) {
                    updateNetworkStateAndConnectionType(context);
                    sendNetworkStateToListeners(context);
                }
                if (event == ManagerEvent.stop) {
                    unregisterBroadcastReceiver(context, this);
                }
                /* no-op event: ManagerEvent.disableNotifications */
                break;
            case OffWithListeners:
                if (event == ManagerEvent.start) {
                    registerBroadcastReceiver(context, this);
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
    private void updateNetworkStateAndConnectionType(final Context context) {
        final ConnectivityManager connectivityManager = (ConnectivityManager) context.getSystemService(
                Context.CONNECTIVITY_SERVICE);
        // Type/status getters below all have a defined behaviour for when connectivityManager == null
        if (connectivityManager == null) {
            Log.e(LOGTAG, "ConnectivityManager does not exist.");
        }
        mCurrentConnectionType = NetworkUtils.getConnectionType(connectivityManager);
        mCurrentNetworkStatus = NetworkUtils.getNetworkStatus(connectivityManager);
        mCurrentConnectionSubtype = NetworkUtils.getConnectionSubType(connectivityManager);
        Log.d(LOGTAG, "New network state: " + mCurrentNetworkStatus + ", " + mCurrentConnectionType + ", " + mCurrentConnectionSubtype);
    }

    @WrapForJNI(dispatchTo = "gecko")
    private static native void onConnectionChanged(int type, String subType,
                                                   boolean isWifi, int dhcpGateway);

    @WrapForJNI(dispatchTo = "gecko")
    private static native void onStatusChanged(String status);

    /**
     * Send current network state and connection type to whomever is listening.
     */
    private void sendNetworkStateToListeners(final Context context) {
        final boolean connectionTypeOrSubtypeChanged = mCurrentConnectionType != mPreviousConnectionType ||
                mCurrentConnectionSubtype != mPreviousConnectionSubtype;
        if (connectionTypeOrSubtypeChanged) {
            mPreviousConnectionType = mCurrentConnectionType;
            mPreviousConnectionSubtype = mCurrentConnectionSubtype;

            final boolean isWifi = mCurrentConnectionType == ConnectionType.WIFI;
            final int gateway = !isWifi ? 0 :
                    wifiDhcpGatewayAddress(context);

            if (GeckoThread.isRunning()) {
                onConnectionChanged(mCurrentConnectionType.value,
                                    mCurrentConnectionSubtype.value, isWifi, gateway);
            } else {
                GeckoThread.queueNativeCall(GeckoNetworkManager.class, "onConnectionChanged",
                                            mCurrentConnectionType.value,
                                            String.class, mCurrentConnectionSubtype.value,
                                            isWifi, gateway);
            }
        }

        // If neither network status nor network configuration changed, do nothing.
        if (mCurrentNetworkStatus == mPreviousNetworkStatus && !connectionTypeOrSubtypeChanged) {
            return;
        }

        // If network status remains the same, send "changed". Otherwise, send new network status.
        // See Bug 1330836 for relevant details.
        final String status;
        if (mCurrentNetworkStatus == mPreviousNetworkStatus) {
            status = LINK_DATA_CHANGED;
        } else {
            mPreviousNetworkStatus = mCurrentNetworkStatus;
            status = mCurrentNetworkStatus.value;
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
    private static void unregisterBroadcastReceiver(final Context context, final BroadcastReceiver receiver) {
        context.unregisterReceiver(receiver);
    }

    /**
     * Start listening for network state updates.
     */
    private static void registerBroadcastReceiver(final Context context, final BroadcastReceiver receiver) {
        final IntentFilter filter = new IntentFilter(ConnectivityManager.CONNECTIVITY_ACTION);
        context.registerReceiver(receiver, filter);
    }

    private static int wifiDhcpGatewayAddress(final Context context) {
        if (context == null) {
            return 0;
        }

        try {
            WifiManager mgr = (WifiManager) context.getApplicationContext().getSystemService(Context.WIFI_SERVICE);
            if (mgr == null) {
                return 0;
            }

            @SuppressLint("MissingPermission") DhcpInfo d = mgr.getDhcpInfo();
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

    @SuppressLint("MissingPermission")
    @Override // BundleEventListener
    /**
     * Handles native messages, not part of the state machine flow.
     */
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
        final Context applicationContext = GeckoAppShell.getApplicationContext();
        switch (event) {
            case "Wifi:GetIPAddress":
                getWifiIPAddress(callback);
                break;
        }
    }

    // This function only works for IPv4; not part of the state machine flow.
    private void getWifiIPAddress(final EventCallback callback) {
        final WifiManager mgr = (WifiManager) GeckoAppShell.getApplicationContext().getSystemService(Context.WIFI_SERVICE);

        if (mgr == null) {
            callback.sendError("Cannot get WifiManager");
            return;
        }

        @SuppressLint("MissingPermission") final WifiInfo info = mgr.getConnectionInfo();
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
}
