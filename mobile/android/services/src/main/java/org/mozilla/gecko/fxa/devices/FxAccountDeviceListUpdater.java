/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.devices;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.net.Uri;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.VisibleForTesting;
import android.util.Log;

import org.mozilla.gecko.background.fxa.FxAccountClient;
import org.mozilla.gecko.background.fxa.FxAccountClient20;
import org.mozilla.gecko.background.fxa.FxAccountClientException;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserContract.RemoteDevices;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.login.State;

import java.util.concurrent.Executor;

public class FxAccountDeviceListUpdater implements FxAccountClient20.RequestDelegate<FxAccountDevice[]> {
    private static final String LOG_TAG = "FxADeviceListUpdater";

    private final AndroidFxAccount fxAccount;
    private final ContentResolver contentResolver;

    public FxAccountDeviceListUpdater(final AndroidFxAccount fxAccount, final ContentResolver cr) {
        this.fxAccount = fxAccount;
        this.contentResolver = cr;
    }

    @Override
    public void handleSuccess(final FxAccountDevice[] result) {
        final Uri uri = RemoteDevices.CONTENT_URI
                        .buildUpon()
                        .appendQueryParameter(BrowserContract.PARAM_PROFILE, fxAccount.getProfile())
                        .build();

        final Bundle valuesBundle = new Bundle();
        final ContentValues[] insertValues = new ContentValues[result.length];

        final long now = System.currentTimeMillis();
        for (int i = 0; i < result.length; i++) {
            final FxAccountDevice fxADevice = result[i];
            final ContentValues deviceValues = new ContentValues();
            deviceValues.put(RemoteDevices.GUID, fxADevice.id);
            deviceValues.put(RemoteDevices.TYPE, fxADevice.type);
            deviceValues.put(RemoteDevices.NAME, fxADevice.name);
            deviceValues.put(RemoteDevices.IS_CURRENT_DEVICE, fxADevice.isCurrentDevice);
            deviceValues.put(RemoteDevices.DATE_CREATED, now);
            deviceValues.put(RemoteDevices.DATE_MODIFIED, now);
            // TODO: Remove that line once FxA sends lastAccessTime all the time.
            final Long lastAccessTime = fxADevice.lastAccessTime != null ? fxADevice.lastAccessTime : 0;
            deviceValues.put(RemoteDevices.LAST_ACCESS_TIME, lastAccessTime);
            insertValues[i] = deviceValues;
        }
        valuesBundle.putParcelableArray(BrowserContract.METHOD_PARAM_DATA, insertValues);
        try {
            contentResolver.call(uri, BrowserContract.METHOD_REPLACE_REMOTE_CLIENTS, uri.toString(),
                                 valuesBundle);
            Log.i(LOG_TAG, "FxA Device list update done.");
        } catch (Exception e) {
            Log.e(LOG_TAG, "Error persisting the new remote device list.", e);
        }
    }

    @Override
    public void handleError(Exception e) {
        onError(e);
    }

    @Override
    public void handleFailure(FxAccountClientException.FxAccountClientRemoteException e) {
        onError(e);
    }

    private void onError(final Exception e) {
        Log.e(LOG_TAG, "Error while getting the FxA device list.", e);
    }

    @VisibleForTesting
    FxAccountClient getSynchronousFxaClient() {
        return new FxAccountClient20(fxAccount.getAccountServerURI(),
                // Current thread executor :)
                new Executor() {
                    @Override
                    public void execute(@NonNull Runnable runnable) {
                        runnable.run();
                    }
                }
        );
    }

    public void update() {
        Log.i(LOG_TAG, "Beginning FxA device list update.");
        final byte[] sessionToken;
        try {
            sessionToken = fxAccount.getState().getSessionToken();
        } catch (State.NotASessionTokenState e) {
            // This should never happen, because the caller (FxAccountSyncAdapter) verifies that
            // we are in a token state before calling this method.
            throw new IllegalStateException("Could not get a session token during Sync (?) " + e);
        }
        final FxAccountClient fxaClient = getSynchronousFxaClient();
        fxaClient.deviceList(sessionToken, this);
    }
}
