/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.mozglue.JNIObject;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.media.MediaCodec;
import android.media.MediaCodec.BufferInfo;
import android.media.MediaFormat;
import android.os.DeadObjectException;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;
import android.view.Surface;

import java.nio.ByteBuffer;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.LinkedList;
import java.util.List;

// Proxy class of ICodec binder.
public final class CodecProxy {
    private static final String LOGTAG = "GeckoRemoteCodecProxy";
    private static final boolean DEBUG = false;

    private static final RemoteManager sRemoteManager = new RemoteManager();

    private ICodec mRemote;
    private FormatParam mFormat;
    private Surface mOutputSurface;
    private CallbacksForwarder mCallbacks;

    public interface Callbacks {
        void onInputExhausted();
        void onOutputFormatChanged(MediaFormat format);
        void onOutput(byte[] bytes, BufferInfo info);
        void onError(boolean fatal);
    }

    @WrapForJNI
    public static class NativeCallbacks extends JNIObject implements Callbacks {
        public native void onInputExhausted();
        public native void onOutputFormatChanged(MediaFormat format);
        public native void onOutput(byte[] bytes, BufferInfo info);
        public native void onError(boolean fatal);

        @Override // JNIObject
        protected native void disposeNative();
    }

    private static class CallbacksForwarder extends ICodecCallbacks.Stub {
        private final Callbacks mCallbacks;
        // // Store the latest frame in case we receive dummy sample.
        private byte[] mPrevBytes;

        CallbacksForwarder(Callbacks callbacks) {
            mCallbacks = callbacks;
        }

        @Override
        public void onInputExhausted() throws RemoteException {
            mCallbacks.onInputExhausted();
        }

        @Override
        public void onOutputFormatChanged(FormatParam format) throws RemoteException {
            mCallbacks.onOutputFormatChanged(format.asFormat());
        }

        @Override
        public void onOutput(Sample sample) throws RemoteException {
            byte[] bytes = null;
            if (sample.isDummy()) { // Dummy sample.
                bytes = mPrevBytes;
            } else {
                mPrevBytes = bytes = sample.getBytes();
            }
            mCallbacks.onOutput(bytes, sample.info);
        }

        @Override
        public void onError(boolean fatal) throws RemoteException {
            reportError(fatal);
        }

        public void reportError(boolean fatal) {
            mCallbacks.onError(fatal);
        }
    }

    private static final class RemoteManager implements IBinder.DeathRecipient {
        private List<CodecProxy> mProxies = new LinkedList<CodecProxy>();
        private ICodecManager mRemote;
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

        public synchronized boolean init() {
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

        public synchronized CodecProxy createCodec(MediaFormat format, Surface surface, Callbacks callbacks) {
            try {
                ICodec remote = mRemote.createCodec();
                CodecProxy proxy = new CodecProxy(format, surface, callbacks);
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
                proxy.mCallbacks.reportError(fatal);
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

        private void releaseCodec(CodecProxy proxy) throws DeadObjectException, RemoteException {
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
    }

    @WrapForJNI
    public static CodecProxy create(MediaFormat format, Surface surface, Callbacks callbacks) {
        if (!sRemoteManager.init()) {
            return null;
        }
        return sRemoteManager.createCodec(format, surface, callbacks);
    }

    private CodecProxy(MediaFormat format, Surface surface, Callbacks callbacks) {
        mFormat = new FormatParam(format);
        mOutputSurface = surface;
        mCallbacks = new CallbacksForwarder(callbacks);
    }

    private boolean init(ICodec remote) {
        try {
            remote.setCallbacks(mCallbacks);
            remote.configure(mFormat, mOutputSurface, 0);
            remote.start();
        } catch (RemoteException e) {
            e.printStackTrace();
            return false;
        }

        mRemote = remote;
        return true;
    }

    private boolean deinit() {
        try {
            mRemote.stop();
            mRemote.release();
            mRemote = null;
            return true;
        } catch (RemoteException e) {
            e.printStackTrace();
            return false;
        }
    }

    @WrapForJNI
    public synchronized boolean input(byte[] bytes, BufferInfo info) {
        if (mRemote == null) {
            Log.e(LOGTAG, "cannot send input to an ended codec");
            return false;
        }
        Sample sample = (info.flags == MediaCodec.BUFFER_FLAG_END_OF_STREAM) ?
                        Sample.EOS : new Sample(ByteBuffer.wrap(bytes), info);
        try {
            mRemote.queueInput(sample);
        } catch (DeadObjectException e) {
            return false;
        } catch (RemoteException e) {
            e.printStackTrace();
            Log.e(LOGTAG, "fail to input sample:" + sample);
            return false;
        }
        return true;
    }

    @WrapForJNI
    public synchronized boolean flush() {
        if (mRemote == null) {
            Log.e(LOGTAG, "cannot flush an ended codec");
            return false;
        }
        try {
            if (DEBUG) Log.d(LOGTAG, "flush " + this);
            mRemote.flush();
        } catch (DeadObjectException e) {
            return false;
        } catch (RemoteException e) {
            e.printStackTrace();
            return false;
        }
        return true;
    }

    @WrapForJNI
    public synchronized boolean release() {
        if (mRemote == null) {
            Log.w(LOGTAG, "codec already ended");
            return true;
        }
        if (DEBUG) Log.d(LOGTAG, "release " + this);
        try {
            sRemoteManager.releaseCodec(this);
        } catch (DeadObjectException e) {
            return false;
        } catch (RemoteException e) {
            e.printStackTrace();
            return false;
        }
        return true;
    }
}
