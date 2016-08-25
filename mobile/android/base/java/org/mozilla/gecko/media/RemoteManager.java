/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import org.mozilla.gecko.GeckoAppShell;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.media.MediaFormat;
import android.os.DeadObjectException;
import android.os.IBinder;
import android.os.RemoteException;
import android.view.Surface;
import android.util.Log;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.LinkedList;
import java.util.List;

public final class RemoteManager implements IBinder.DeathRecipient {
    private static final String LOGTAG = "GeckoRemoteManager";
    private static final boolean DEBUG = false;
    private static RemoteManager sRemoteManager = null;

    public synchronized static RemoteManager getInstance() {
        if (sRemoteManager == null){
            sRemoteManager = new RemoteManager();
        }

        sRemoteManager.init();
        return sRemoteManager;
    }

    private List<CodecProxy> mProxies = new LinkedList<CodecProxy>();
    private volatile ICodecManager mRemote;
    private volatile CountDownLatch mConnectionLatch;
    private final ServiceConnection mConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            if (DEBUG) Log.d(LOGTAG, "service connected");
            try {
                service.linkToDeath(RemoteManager.this, 0);
            } catch (RemoteException e) {
                e.printStackTrace();
            }
            mRemote = ICodecManager.Stub.asInterface(service);
            if (mConnectionLatch != null) {
                mConnectionLatch.countDown();
            }
        }

        /**
         * Called when a connection to the Service has been lost.  This typically
         * happens when the process hosting the service has crashed or been killed.
         * This does <em>not</em> remove the ServiceConnection itself -- this
         * binding to the service will remain active, and you will receive a call
         * to {@link #onServiceConnected} when the Service is next running.
         *
         * @param name The concrete component name of the service whose
         *             connection has been lost.
         */
        @Override
        public void onServiceDisconnected(ComponentName name) {
            if (DEBUG) Log.d(LOGTAG, "service disconnected");
            mRemote.asBinder().unlinkToDeath(RemoteManager.this, 0);
            mRemote = null;
            if (mConnectionLatch != null) {
                mConnectionLatch.countDown();
            }
        }
    };

    private synchronized boolean init() {
        if (mRemote != null) {
            return true;
        }

        if (DEBUG) Log.d(LOGTAG, "init remote manager " + this);
        Context appCtxt = GeckoAppShell.getApplicationContext();
        if (DEBUG) Log.d(LOGTAG, "ctxt=" + appCtxt);
        appCtxt.bindService(new Intent(appCtxt, CodecManager.class),
                mConnection, Context.BIND_AUTO_CREATE);
        if (!waitConnection()) {
            appCtxt.unbindService(mConnection);
            return false;
        }
        return true;
    }

    private boolean waitConnection() {
        boolean ok = false;

        mConnectionLatch = new CountDownLatch(1);
        try {
            int retryCount = 0;
            while (retryCount < 5) {
                if (DEBUG) Log.d(LOGTAG, "waiting for connection latch:" + mConnectionLatch);
                mConnectionLatch.await(1, TimeUnit.SECONDS);
                if (mConnectionLatch.getCount() == 0) {
                    break;
                }
                Log.w(LOGTAG, "Creator not connected in 1s. Try again.");
                retryCount++;
            }
            ok = true;
        } catch (InterruptedException e) {
            Log.e(LOGTAG, "service not connected in 5 seconds. Stop waiting.");
            e.printStackTrace();
        }
        mConnectionLatch = null;

        return ok;
    }

    public synchronized CodecProxy createCodec(MediaFormat format,
                                               Surface surface,
                                               CodecProxy.Callbacks callbacks) {
        if (mRemote == null) {
            if (DEBUG) Log.d(LOGTAG, "createCodec failed due to not initialize");
            return null;
        }
        try {
            ICodec remote = mRemote.createCodec();
            CodecProxy proxy = CodecProxy.createCodecProxy(format, surface, callbacks);
            if (proxy.init(remote)) {
                mProxies.add(proxy);
                return proxy;
            } else {
                return null;
            }
        } catch (RemoteException e) {
            e.printStackTrace();
            return null;
        }
    }

    @Override
    public void binderDied() {
        Log.e(LOGTAG, "remote codec is dead");
        handleRemoteDeath();
    }

    private synchronized void handleRemoteDeath() {
        // Wait for onServiceDisconnected()
        if (!waitConnection()) {
            notifyError(true);
            return;
        }
        // Restart
        if (init() && recoverRemoteCodec()) {
            notifyError(false);
        } else {
            notifyError(true);
        }
    }

    private synchronized void notifyError(boolean fatal) {
        for (CodecProxy proxy : mProxies) {
            proxy.reportError(fatal);
        }
    }

    private synchronized boolean recoverRemoteCodec() {
        if (DEBUG) Log.d(LOGTAG, "recover codec");
        boolean ok = true;
        try {
            for (CodecProxy proxy : mProxies) {
                ok &= proxy.init(mRemote.createCodec());
            }
            return ok;
        } catch (RemoteException e) {
            return false;
        }
    }

    public void releaseCodec(CodecProxy proxy) throws DeadObjectException, RemoteException {
        if (mRemote == null) {
            if (DEBUG) Log.d(LOGTAG, "releaseCodec called but not initialized yet");
            return;
        }
        proxy.deinit();
        synchronized (this) {
            if (mProxies.remove(proxy) && mProxies.isEmpty()) {
                release();
            }
        }
    }

    private void release() {
        if (DEBUG) Log.d(LOGTAG, "release remote manager " + this);
        Context appCtxt = GeckoAppShell.getApplicationContext();
        mRemote.asBinder().unlinkToDeath(this, 0);
        mRemote = null;
        appCtxt.unbindService(mConnection);
    }
} // RemoteManager