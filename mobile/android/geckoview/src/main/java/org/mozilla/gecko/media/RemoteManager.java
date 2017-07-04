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

import java.util.LinkedList;
import java.util.List;
import java.util.NoSuchElementException;

import org.mozilla.gecko.gfx.GeckoSurface;

public final class RemoteManager implements IBinder.DeathRecipient {
    private static final String LOGTAG = "GeckoRemoteManager";
    private static final boolean DEBUG = false;
    private static RemoteManager sRemoteManager = null;
    private static ICrashReporter setCrashReporter = null;

    public synchronized static RemoteManager getInstance() {
        if (sRemoteManager == null) {
            sRemoteManager = new RemoteManager();
        }

        sRemoteManager.init();
        return sRemoteManager;
    }

    private List<CodecProxy> mProxies = new LinkedList<CodecProxy>();
    private volatile IMediaManager mRemote;

    private final class RemoteConnection implements ServiceConnection {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            if (DEBUG) Log.d(LOGTAG, "service connected");
            try {
                service.linkToDeath(RemoteManager.this, 0);
            } catch (RemoteException e) {
                e.printStackTrace();
            }
            synchronized (this) {
                mRemote = IMediaManager.Stub.asInterface(service);
                notify();
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            if (DEBUG) Log.d(LOGTAG, "service disconnected");
            try {
                mRemote.asBinder().unlinkToDeath(RemoteManager.this, 0);
            } catch (NoSuchElementException e) {
                Log.w(LOGTAG, "death recipient already released");
            }
            synchronized (this) {
                mRemote = null;
                notify();
            }
        }

        private boolean connect() {
            Context appCtxt = GeckoAppShell.getApplicationContext();
            appCtxt.bindService(new Intent(appCtxt, MediaManager.class),
                    mConnection, Context.BIND_AUTO_CREATE);
            waitConnect();
            return mRemote != null;
        }

        // Wait up to 5s.
        private synchronized void waitConnect() {
            int waitCount = 0;
            while (mRemote == null && waitCount < 5) {
                try {
                    wait(1000);
                    waitCount++;
                } catch (InterruptedException e) {
                    if (DEBUG) { e.printStackTrace(); }
                }
            }
            if (DEBUG) {
                Log.d(LOGTAG, "wait ~" + waitCount + "s for connection: " + (mRemote == null ? "fail" : "ok"));
            }
        }

        private synchronized void waitDisconnect() {
            while (mRemote != null) {
                try {
                    wait(1000);
                } catch (InterruptedException e) {
                    if (DEBUG) { e.printStackTrace(); }
                }
            }
        }
    };

    RemoteConnection mConnection = new RemoteConnection();

    private synchronized boolean init() {
        if (mRemote != null) {
            return true;
        }

        if (DEBUG) Log.d(LOGTAG, "init remote manager " + this);
        return mConnection.connect();
    }

    public synchronized CodecProxy createCodec(boolean isEncoder,
                                               MediaFormat format,
                                               GeckoSurface surface,
                                               CodecProxy.Callbacks callbacks,
                                               String drmStubId) {
        if (mRemote == null) {
            if (DEBUG) Log.d(LOGTAG, "createCodec failed due to not initialize");
            return null;
        }
        try {
            ICodec remote = mRemote.createCodec();
            CodecProxy proxy = CodecProxy.createCodecProxy(isEncoder, format, surface, callbacks, drmStubId);
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

    private void reportDecodingProcessCrash() {
        if (setCrashReporter != null) {
            setCrashReporter.reportDecodingProcessCrash();
        }
    }

    public static void setCrashReporter(ICrashReporter reporter) {
        setCrashReporter = reporter;
    }

    public interface ICrashReporter {
        void reportDecodingProcessCrash();
    }

    public synchronized IMediaDrmBridge createRemoteMediaDrmBridge(String keySystem,
                                                                   String stubId) {
        if (mRemote == null) {
            if (DEBUG) Log.d(LOGTAG, "createRemoteMediaDrmBridge failed due to not initialize");
            return null;
        }
        try {
            IMediaDrmBridge remoteBridge =
                mRemote.createRemoteMediaDrmBridge(keySystem, stubId);
            return remoteBridge;
        } catch (RemoteException e) {
            Log.e(LOGTAG, "Got exception during createRemoteMediaDrmBridge().", e);
            return null;
        }
    }

    @Override
    public void binderDied() {
        Log.e(LOGTAG, "remote codec is dead");
        reportDecodingProcessCrash();
        handleRemoteDeath();
    }

    private synchronized void handleRemoteDeath() {
        mConnection.waitDisconnect();

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