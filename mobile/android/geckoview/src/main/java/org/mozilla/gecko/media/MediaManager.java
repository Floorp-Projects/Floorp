/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;
import android.os.Process;
import android.os.RemoteException;
import android.util.Log;

import org.mozilla.gecko.mozglue.GeckoLoader;

public final class MediaManager extends Service {
    private static final String LOGTAG = "GeckoMediaManager";
    private static boolean sNativeLibLoaded;

    private Binder mBinder = new IMediaManager.Stub() {
        @Override
        public ICodec createCodec() throws RemoteException {
            return new Codec();
        }

        @Override
        public IMediaDrmBridge createRemoteMediaDrmBridge(final String keySystem,
                                                          final String stubId)
            throws RemoteException {
            return new RemoteMediaDrmBridgeStub(keySystem, stubId);
        }
    };

    @Override
    public synchronized void onCreate() {
        if (!sNativeLibLoaded) {
            GeckoLoader.doLoadLibrary(this, "mozglue");
            GeckoLoader.suppressCrashDialog();
            sNativeLibLoaded = true;
        }
    }

    @Override
    public IBinder onBind(final Intent intent) {
        return mBinder;
    }

    @Override
    public boolean onUnbind(final Intent intent) {
        Log.i(LOGTAG, "Media service has been unbound. Stopping.");
        stopSelf();
        Process.killProcess(Process.myPid());
        return false;
    }
}
