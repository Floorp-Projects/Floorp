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
import android.os.RemoteException;
import android.util.Log;
import android.view.Surface;

import java.nio.ByteBuffer;
// Proxy class of ICodec binder.
public final class CodecProxy {
    private static final String LOGTAG = "GeckoRemoteCodecProxy";
    private static final boolean DEBUG = false;

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

    @WrapForJNI
    public static CodecProxy create(MediaFormat format, Surface surface, Callbacks callbacks) {
        return RemoteManager.getInstance().createCodec(format, surface, callbacks);
    }

    public static CodecProxy createCodecProxy(MediaFormat format, Surface surface, Callbacks callbacks) {
        return new CodecProxy(format, surface, callbacks);
    }

    private CodecProxy(MediaFormat format, Surface surface, Callbacks callbacks) {
        mFormat = new FormatParam(format);
        mOutputSurface = surface;
        mCallbacks = new CallbacksForwarder(callbacks);
    }

    boolean init(ICodec remote) {
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

    boolean deinit() {
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
            RemoteManager.getInstance().releaseCodec(this);
        } catch (DeadObjectException e) {
            return false;
        } catch (RemoteException e) {
            e.printStackTrace();
            return false;
        }
        return true;
    }

    public synchronized void reportError(boolean fatal) {
        mCallbacks.reportError(fatal);
    }
}
