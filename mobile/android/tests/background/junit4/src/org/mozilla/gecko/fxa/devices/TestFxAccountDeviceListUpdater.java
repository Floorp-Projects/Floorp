/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.fxa.devices;

import android.content.ContentProviderClient;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.RemoteException;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mozilla.gecko.background.db.DelegatingTestContentProvider;
import org.mozilla.gecko.background.fxa.FxAccountClient;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserProvider;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.login.State;
import org.robolectric.shadows.ShadowContentResolver;

import java.util.List;
import java.util.UUID;

import static java.util.Objects.deepEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

@RunWith(TestRunner.class)
public class TestFxAccountDeviceListUpdater {

    @Before
    public void init() {
        // Process Mockito annotations
        MockitoAnnotations.initMocks(this);
        fxaDevicesUpdater = spy(new FxAccountDeviceListUpdater(fxAccount, contentResolver));
    }

    FxAccountDeviceListUpdater fxaDevicesUpdater;

    @Mock
    AndroidFxAccount fxAccount;

    @Mock
    State state;

    @Mock
    ContentResolver contentResolver;

    @Mock
    FxAccountClient fxaClient;

    @Test
    public void testUpdate() throws Throwable {
        byte[] token = "usertoken".getBytes();

        when(fxAccount.getState()).thenReturn(state);
        when(state.getSessionToken()).thenReturn(token);
        doReturn(fxaClient).when(fxaDevicesUpdater).getSynchronousFxaClient();

        fxaDevicesUpdater.update();
        verify(fxaClient).deviceList(token, fxaDevicesUpdater);
    }

    @Test
    public void testSuccessHandler() throws Throwable {
        FxAccountDevice[] result = new FxAccountDevice[2];
        FxAccountDevice device1 = new FxAccountDevice("Current device", "deviceid1", "mobile", true, System.currentTimeMillis(),
                "https://localhost/push/callback1", "abc123", "321cba");
        FxAccountDevice device2 = new FxAccountDevice("Desktop PC", "deviceid2", "desktop", true, System.currentTimeMillis(),
                "https://localhost/push/callback2", "abc123", "321cba");
        result[0] = device1;
        result[1] = device2;

        when(fxAccount.getProfile()).thenReturn("default");

        long timeBeforeCall = System.currentTimeMillis();
        fxaDevicesUpdater.handleSuccess(result);

        ArgumentCaptor<Bundle> captor = ArgumentCaptor.forClass(Bundle.class);
        verify(contentResolver).call(any(Uri.class), eq(BrowserContract.METHOD_REPLACE_REMOTE_CLIENTS), anyString(), captor.capture());
        List<Bundle> allArgs = captor.getAllValues();
        assertTrue(allArgs.size() == 1);
        ContentValues[] allValues = (ContentValues[]) allArgs.get(0).getParcelableArray(BrowserContract.METHOD_PARAM_DATA);

        ContentValues firstDevice = allValues[0];
        checkInsertDeviceContentValues(device1, firstDevice, timeBeforeCall);
        ContentValues secondDevice = allValues[1];
        checkInsertDeviceContentValues(device2, secondDevice, timeBeforeCall);
    }

    private void checkInsertDeviceContentValues(FxAccountDevice device, ContentValues firstDevice, long timeBeforeCall) {
        assertEquals(firstDevice.getAsString(BrowserContract.RemoteDevices.GUID), device.id);
        assertEquals(firstDevice.getAsString(BrowserContract.RemoteDevices.TYPE), device.type);
        assertEquals(firstDevice.getAsString(BrowserContract.RemoteDevices.NAME), device.name);
        assertEquals(firstDevice.getAsBoolean(BrowserContract.RemoteDevices.IS_CURRENT_DEVICE), device.isCurrentDevice);
        deepEquals(firstDevice.getAsString(BrowserContract.RemoteDevices.LAST_ACCESS_TIME), device.lastAccessTime);
        assertTrue(firstDevice.getAsLong(BrowserContract.RemoteDevices.DATE_CREATED) < timeBeforeCall + 10000); // Give 10 secs of leeway
        assertTrue(firstDevice.getAsLong(BrowserContract.RemoteDevices.DATE_MODIFIED) < timeBeforeCall + 10000);
        assertEquals(firstDevice.getAsString(BrowserContract.RemoteDevices.NAME), device.name);
    }

    @Test
    public void testBrowserProvider() {
        Uri uri = testUri(BrowserContract.RemoteDevices.CONTENT_URI);

        BrowserProvider provider = new BrowserProvider();
        Cursor c = null;
        try {
            provider.onCreate();
            ShadowContentResolver.registerProvider(BrowserContract.AUTHORITY, new DelegatingTestContentProvider(provider));

            final ShadowContentResolver cr = new ShadowContentResolver();
            ContentProviderClient remoteDevicesClient = cr.acquireContentProviderClient(BrowserContract.RemoteDevices.CONTENT_URI);

            // First let's insert a client for initial state.

            Bundle bundle = new Bundle();
            ContentValues device1 = createMockRemoteClientValues("device1");
            bundle.putParcelableArray(BrowserContract.METHOD_PARAM_DATA, new ContentValues[] { device1 });

            remoteDevicesClient.call(BrowserContract.METHOD_REPLACE_REMOTE_CLIENTS, uri.toString(), bundle);

            c = remoteDevicesClient.query(uri, null, null, null, "name ASC");
            assertEquals(c.getCount(), 1);
            c.moveToFirst();
            int nameCol = c.getColumnIndexOrThrow("name");
            assertEquals(c.getString(nameCol), "device1");
            c.close();

            // Then we replace our remote clients list with a new one.

            bundle = new Bundle();
            ContentValues device2 = createMockRemoteClientValues("device2");
            ContentValues device3 = createMockRemoteClientValues("device3");
            bundle.putParcelableArray(BrowserContract.METHOD_PARAM_DATA, new ContentValues[] { device2, device3 });

            remoteDevicesClient.call(BrowserContract.METHOD_REPLACE_REMOTE_CLIENTS, uri.toString(), bundle);

            c = remoteDevicesClient.query(uri, null, null, null, "name ASC");
            assertEquals(c.getCount(), 2);
            c.moveToFirst();
            nameCol = c.getColumnIndexOrThrow("name");
            assertEquals(c.getString(nameCol), "device2");
            c.moveToNext();
            assertEquals(c.getString(nameCol), "device3");
            c.close();
        } catch (RemoteException e) {
            fail(e.getMessage());
        } finally {
            if (c != null && !c.isClosed()) {
                c.close();
            }
            provider.shutdown();
        }
    }

    private Uri testUri(Uri baseUri) {
        return baseUri.buildUpon().appendQueryParameter(BrowserContract.PARAM_IS_TEST, "1").build();
    }

    private ContentValues createMockRemoteClientValues(String name) {
        final long now = System.currentTimeMillis();
        ContentValues cli = new ContentValues();
        cli.put(BrowserContract.RemoteDevices.GUID, UUID.randomUUID().toString());
        cli.put(BrowserContract.RemoteDevices.NAME, name);
        cli.put(BrowserContract.RemoteDevices.TYPE, "mobile");
        cli.put(BrowserContract.RemoteDevices.IS_CURRENT_DEVICE, false);
        cli.put(BrowserContract.RemoteDevices.LAST_ACCESS_TIME, System.currentTimeMillis());
        cli.put(BrowserContract.RemoteDevices.DATE_CREATED, now);
        cli.put(BrowserContract.RemoteDevices.DATE_MODIFIED, now);
        return cli;
    }
}
