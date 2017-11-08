/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.util;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.telephony.TelephonyManager;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.util.NetworkUtils.ConnectionSubType;
import org.mozilla.gecko.util.NetworkUtils.ConnectionType;
import org.mozilla.gecko.util.NetworkUtils.NetworkStatus;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowConnectivityManager;
import org.robolectric.shadows.ShadowNetworkInfo;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

@RunWith(TestRunner.class)
public class NetworkUtilsTest {
    private ConnectivityManager connectivityManager;
    private ShadowConnectivityManager shadowConnectivityManager;

    @Before
    public void setUp() {
        connectivityManager = (ConnectivityManager) RuntimeEnvironment.application.getSystemService(Context.CONNECTIVITY_SERVICE);

        // Not using Shadows.shadowOf(connectivityManager) because of Robolectric bug when using API23+
        // See: https://github.com/robolectric/robolectric/issues/1862
        shadowConnectivityManager = (ShadowConnectivityManager) Shadow.extract(connectivityManager);
    }

    @Test
    public void testIsConnected() throws Exception {
        assertFalse(NetworkUtils.isConnected((ConnectivityManager) null));

        shadowConnectivityManager.setActiveNetworkInfo(null);
        assertFalse(NetworkUtils.isConnected(connectivityManager));

        shadowConnectivityManager.setActiveNetworkInfo(
                ShadowNetworkInfo.newInstance(NetworkInfo.DetailedState.CONNECTED, ConnectivityManager.TYPE_WIFI, 0, true, true)
        );
        assertTrue(NetworkUtils.isConnected(connectivityManager));

        shadowConnectivityManager.setActiveNetworkInfo(
                ShadowNetworkInfo.newInstance(NetworkInfo.DetailedState.DISCONNECTED, ConnectivityManager.TYPE_WIFI, 0, true, false)
        );
        assertFalse(NetworkUtils.isConnected(connectivityManager));
    }

    @Test
    public void testGetConnectionSubType() throws Exception {
        assertEquals(ConnectionSubType.UNKNOWN, NetworkUtils.getConnectionSubType(null));

        shadowConnectivityManager.setActiveNetworkInfo(null);
        assertEquals(ConnectionSubType.UNKNOWN, NetworkUtils.getConnectionSubType(connectivityManager));

        // We don't seem to care about figuring out all connection types. So...
        shadowConnectivityManager.setActiveNetworkInfo(
                ShadowNetworkInfo.newInstance(NetworkInfo.DetailedState.CONNECTED, ConnectivityManager.TYPE_VPN, 0, true, true)
        );
        assertEquals(ConnectionSubType.UNKNOWN, NetworkUtils.getConnectionSubType(connectivityManager));

        // But anything below we should recognize.
        shadowConnectivityManager.setActiveNetworkInfo(
                ShadowNetworkInfo.newInstance(NetworkInfo.DetailedState.CONNECTED, ConnectivityManager.TYPE_ETHERNET, 0, true, true)
        );
        assertEquals(ConnectionSubType.ETHERNET, NetworkUtils.getConnectionSubType(connectivityManager));

        shadowConnectivityManager.setActiveNetworkInfo(
                ShadowNetworkInfo.newInstance(NetworkInfo.DetailedState.CONNECTED, ConnectivityManager.TYPE_WIFI, 0, true, true)
        );
        assertEquals(ConnectionSubType.WIFI, NetworkUtils.getConnectionSubType(connectivityManager));

        shadowConnectivityManager.setActiveNetworkInfo(
                ShadowNetworkInfo.newInstance(NetworkInfo.DetailedState.CONNECTED, ConnectivityManager.TYPE_WIMAX, 0, true, true)
        );
        assertEquals(ConnectionSubType.WIMAX, NetworkUtils.getConnectionSubType(connectivityManager));

        // Unknown mobile
        shadowConnectivityManager.setActiveNetworkInfo(
                ShadowNetworkInfo.newInstance(NetworkInfo.DetailedState.CONNECTED, ConnectivityManager.TYPE_MOBILE, TelephonyManager.NETWORK_TYPE_UNKNOWN, true, true)
        );
        assertEquals(ConnectionSubType.UNKNOWN, NetworkUtils.getConnectionSubType(connectivityManager));

        // 2G mobile types
        int[] cell2gTypes = new int[] {
                TelephonyManager.NETWORK_TYPE_GPRS,
                TelephonyManager.NETWORK_TYPE_EDGE,
                TelephonyManager.NETWORK_TYPE_CDMA,
                TelephonyManager.NETWORK_TYPE_1xRTT,
                TelephonyManager.NETWORK_TYPE_IDEN
        };
        for (int i = 0; i < cell2gTypes.length; i++) {
            shadowConnectivityManager.setActiveNetworkInfo(
                    ShadowNetworkInfo.newInstance(NetworkInfo.DetailedState.CONNECTED, ConnectivityManager.TYPE_MOBILE, cell2gTypes[i], true, true)
            );
            assertEquals(ConnectionSubType.CELL_2G, NetworkUtils.getConnectionSubType(connectivityManager));
        }

        // 3G mobile types
        int[] cell3gTypes = new int[] {
                TelephonyManager.NETWORK_TYPE_UMTS,
                TelephonyManager.NETWORK_TYPE_EVDO_0,
                TelephonyManager.NETWORK_TYPE_EVDO_A,
                TelephonyManager.NETWORK_TYPE_HSDPA,
                TelephonyManager.NETWORK_TYPE_HSUPA,
                TelephonyManager.NETWORK_TYPE_HSPA,
                TelephonyManager.NETWORK_TYPE_EVDO_B,
                TelephonyManager.NETWORK_TYPE_EHRPD,
                TelephonyManager.NETWORK_TYPE_HSPAP
        };
        for (int i = 0; i < cell3gTypes.length; i++) {
            shadowConnectivityManager.setActiveNetworkInfo(
                    ShadowNetworkInfo.newInstance(NetworkInfo.DetailedState.CONNECTED, ConnectivityManager.TYPE_MOBILE, cell3gTypes[i], true, true)
            );
            assertEquals(ConnectionSubType.CELL_3G, NetworkUtils.getConnectionSubType(connectivityManager));
        }

        // 4G mobile type
        shadowConnectivityManager.setActiveNetworkInfo(
                ShadowNetworkInfo.newInstance(NetworkInfo.DetailedState.CONNECTED, ConnectivityManager.TYPE_MOBILE, TelephonyManager.NETWORK_TYPE_LTE, true, true)
        );
        assertEquals(ConnectionSubType.CELL_4G, NetworkUtils.getConnectionSubType(connectivityManager));
    }

    @Test
    public void testGetConnectionType() {
        shadowConnectivityManager.setActiveNetworkInfo(null);
        assertEquals(ConnectionType.NONE, NetworkUtils.getConnectionType(connectivityManager));
        assertEquals(ConnectionType.NONE, NetworkUtils.getConnectionType(null));

        shadowConnectivityManager.setActiveNetworkInfo(
                ShadowNetworkInfo.newInstance(NetworkInfo.DetailedState.CONNECTED, ConnectivityManager.TYPE_VPN, 0, true, true)
        );
        assertEquals(ConnectionType.OTHER, NetworkUtils.getConnectionType(connectivityManager));

        shadowConnectivityManager.setActiveNetworkInfo(
                ShadowNetworkInfo.newInstance(NetworkInfo.DetailedState.CONNECTED, ConnectivityManager.TYPE_WIFI, 0, true, true)
        );
        assertEquals(ConnectionType.WIFI, NetworkUtils.getConnectionType(connectivityManager));

        shadowConnectivityManager.setActiveNetworkInfo(
                ShadowNetworkInfo.newInstance(NetworkInfo.DetailedState.CONNECTED, ConnectivityManager.TYPE_MOBILE, 0, true, true)
        );
        assertEquals(ConnectionType.CELLULAR, NetworkUtils.getConnectionType(connectivityManager));

        shadowConnectivityManager.setActiveNetworkInfo(
                ShadowNetworkInfo.newInstance(NetworkInfo.DetailedState.CONNECTED, ConnectivityManager.TYPE_ETHERNET, 0, true, true)
        );
        assertEquals(ConnectionType.ETHERNET, NetworkUtils.getConnectionType(connectivityManager));

        shadowConnectivityManager.setActiveNetworkInfo(
                ShadowNetworkInfo.newInstance(NetworkInfo.DetailedState.CONNECTED, ConnectivityManager.TYPE_BLUETOOTH, 0, true, true)
        );
        assertEquals(ConnectionType.BLUETOOTH, NetworkUtils.getConnectionType(connectivityManager));

        shadowConnectivityManager.setActiveNetworkInfo(
                ShadowNetworkInfo.newInstance(NetworkInfo.DetailedState.CONNECTED, ConnectivityManager.TYPE_WIMAX, 0, true, true)
        );
        assertEquals(ConnectionType.CELLULAR, NetworkUtils.getConnectionType(connectivityManager));
    }

    @Test
    public void testGetNetworkStatus() {
        assertEquals(NetworkStatus.UNKNOWN, NetworkUtils.getNetworkStatus(null));

        shadowConnectivityManager.setActiveNetworkInfo(null);
        assertEquals(NetworkStatus.DOWN, NetworkUtils.getNetworkStatus(connectivityManager));

        shadowConnectivityManager.setActiveNetworkInfo(
                ShadowNetworkInfo.newInstance(NetworkInfo.DetailedState.CONNECTING, ConnectivityManager.TYPE_MOBILE, 0, true, false)
        );
        assertEquals(NetworkStatus.DOWN, NetworkUtils.getNetworkStatus(connectivityManager));

        shadowConnectivityManager.setActiveNetworkInfo(
                ShadowNetworkInfo.newInstance(NetworkInfo.DetailedState.CONNECTED, ConnectivityManager.TYPE_MOBILE, 0, true, true)
        );
        assertEquals(NetworkStatus.UP, NetworkUtils.getNetworkStatus(connectivityManager));
    }
}
