/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.media.MediaCodec;
import android.media.MediaCodec.BufferInfo;
import android.media.MediaCodec.CryptoInfo;
import android.media.MediaFormat;
import android.os.DeadObjectException;
import android.os.RemoteException;
import android.util.Log;
import android.view.Surface;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.mozglue.JNIObject;

import java.io.IOException;
import java.nio.ByteBuffer;

// Proxy class of ICodec binder.
public final class CodecProxy {
    private static final String LOGTAG = "GeckoRemoteCodecProxy";
    private static final boolean DEBUG = false;

    private ICodec mRemote;
    private FormatParam mFormat;
    private Surface mOutputSurface;
    private CallbacksForwarder mCallbacks;
    private String mRemoteDrmStubId;

    public interface Callbacks {
        void onInputExhausted();
        void onOutputFormatChanged(MediaFormat format);
        void onOutput(Sample output);
        void onError(boolean fatal);
    }

    @WrapForJNI
    public static class NativeCallbacks extends JNIObject implements Callbacks {
        public native void onInputExhausted();
        public native void onOutputFormatChanged(MediaFormat format);
        public native void onOutput(Sample output);
        public native void onError(boolean fatal);

        @Override // JNIObject
        protected native void disposeNative();
    }

    private class CallbacksForwarder extends ICodecCallbacks.Stub {
        private final Callbacks mCallbacks;

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
            mCallbacks.onOutput(sample);
            mRemote.releaseOutput(sample);
            sample.dispose();
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
    public static CodecProxy create(MediaFormat format,
                                    Surface surface,
                                    Callbacks callbacks,
                                    String drmStubId) {
        return RemoteManager.getInstance().createCodec(format, surface, callbacks, drmStubId);
    }

    public static CodecProxy createCodecProxy(MediaFormat format,
                                              Surface surface,
                                              Callbacks callbacks,
                                              String drmStubId) {
        return new CodecProxy(format, surface, callbacks, drmStubId);
    }

    private CodecProxy(MediaFormat format, Surface surface, Callbacks callbacks, String drmStubId) {
        mFormat = new FormatParam(format);
        mOutputSurface = surface;
        mRemoteDrmStubId = drmStubId;
        mCallbacks = new CallbacksForwarder(callbacks);
    }

    boolean init(ICodec remote) {
        try {
            remote.setCallbacks(mCallbacks);
            remote.configure(mFormat, mOutputSurface, 0, mRemoteDrmStubId);
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
    public synchronized boolean isAdaptivePlaybackSupported()
    {
      if (mRemote == null) {
          Log.e(LOGTAG, "cannot check isAdaptivePlaybackSupported with an ended codec");
          return false;
      }
      try {
            return mRemote.isAdaptivePlaybackSupported();
        } catch (RemoteException e) {
            e.printStackTrace();
            return false;
        }
    }

    @WrapForJNI
    public synchronized boolean input(ByteBuffer bytes, BufferInfo info, CryptoInfo cryptoInfo) {
        if (mRemote == null) {
            Log.e(LOGTAG, "cannot send input to an ended codec");
            return false;
        }
        try {
            Sample sample = (info.flags == MediaCodec.BUFFER_FLAG_END_OF_STREAM) ?
                    Sample.EOS : mRemote.dequeueInput(info.size).set(bytes, info, cryptoInfo);
            mRemote.queueInput(sample);
            sample.dispose();
        } catch (IOException e) {
            e.printStackTrace();
            return false;
        } catch (DeadObjectException e) {
            return false;
        } catch (RemoteException e) {
            e.printStackTrace();
            Log.e(LOGTAG, "fail to input sample: size=" + info.size +
                    ", pts=" + info.presentationTimeUs +
                    ", flags=" + Integer.toHexString(info.flags));
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
