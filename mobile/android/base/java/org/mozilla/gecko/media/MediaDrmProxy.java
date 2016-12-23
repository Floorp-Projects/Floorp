/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


package org.mozilla.gecko.media;

import java.util.ArrayList;
import java.util.UUID;

import org.mozilla.gecko.mozglue.JNIObject;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.AppConstants;

import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.media.MediaCrypto;
import android.media.MediaDrm;
import android.util.Log;
import android.os.Build;

public final class MediaDrmProxy {
    private static final String LOGTAG = "GeckoMediaDrmProxy";
    private static final boolean DEBUG = false;
    private static final UUID WIDEVINE_SCHEME_UUID =
            new UUID(0xedef8ba979d64aceL, 0xa3c827dcd51d21edL);

    private static final String WIDEVINE_KEY_SYSTEM = "com.widevine.alpha";
    @WrapForJNI
    private static final String AAC = "audio/mp4a-latm";
    @WrapForJNI
    private static final String AVC = "video/avc";
    @WrapForJNI
    private static final String VORBIS = "audio/vorbis";
    @WrapForJNI
    private static final String VP8 = "video/x-vnd.on2.vp8";
    @WrapForJNI
    private static final String VP9 = "video/x-vnd.on2.vp9";
    @WrapForJNI
    private static final String OPUS = "audio/opus";

    public static final ArrayList<MediaDrmProxy> sProxyList = new ArrayList<MediaDrmProxy>();

    // A flag to avoid using the native object that has been destroyed.
    private boolean mDestroyed;
    private GeckoMediaDrm mImpl;
    private String mDrmStubId;

    private static boolean isSystemSupported() {
        // Support versions >= Marshmallow
        if (AppConstants.Versions.preMarshmallow) {
            if (DEBUG) Log.d(LOGTAG, "System Not supported !!, current SDK version is " + Build.VERSION.SDK_INT);
            return false;
        }
        return true;
    }

    @WrapForJNI
    public static boolean isSchemeSupported(String keySystem) {
        if (!isSystemSupported()) {
            return false;
        }
        if (keySystem.equals(WIDEVINE_KEY_SYSTEM)) {
            return MediaDrm.isCryptoSchemeSupported(WIDEVINE_SCHEME_UUID)
                    && MediaCrypto.isCryptoSchemeSupported(WIDEVINE_SCHEME_UUID);
        }
        if (DEBUG) Log.d(LOGTAG, "isSchemeSupported key sytem = " + keySystem);
        return false;
    }

    @WrapForJNI
    public static boolean IsCryptoSchemeSupported(String keySystem,
                                                  String container) {
        if (!isSystemSupported()) {
            return false;
        }
        if (keySystem.equals(WIDEVINE_KEY_SYSTEM)) {
            return MediaDrm.isCryptoSchemeSupported(WIDEVINE_SCHEME_UUID, container);
        }
        if (DEBUG) Log.d(LOGTAG, "cannot decrypt key sytem = " + keySystem + ", container = " + container);
        return false;
    }

    @WrapForJNI
    public static boolean CanDecode(String mimeType) {
        for (int i = 0; i < MediaCodecList.getCodecCount(); ++i) {
            MediaCodecInfo info = MediaCodecList.getCodecInfoAt(i);
            if (info.isEncoder()) {
                continue;
            }
            for (String m : info.getSupportedTypes()) {
                if (m.equals(mimeType)) {
                  return true;
                }
            }
        }
        if (DEBUG) Log.d(LOGTAG, "cannot decode mimetype = " + mimeType);
        return false;
    }

     // Interface for callback to native.
    public interface Callbacks {
        void onSessionCreated(int createSessionToken,
                              int promiseId,
                              byte[] sessionId,
                              byte[] request);

        void onSessionUpdated(int promiseId, byte[] sessionId);

        void onSessionClosed(int promiseId, byte[] sessionId);

        void onSessionMessage(byte[] sessionId,
                              int sessionMessageType,
                              byte[] request);

       void onSessionError(byte[] sessionId,
                           String message);

        // MediaDrm.KeyStatus is available in API level 23(M)
        // https://developer.android.com/reference/android/media/MediaDrm.KeyStatus.html
        // For compatibility between L and M above, we'll unwrap the KeyStatus structure
        // and store the keyid and status into SessionKeyInfo and pass to native(MediaDrmCDMProxy).
        void onSessionBatchedKeyChanged(byte[] sessionId,
                                        SessionKeyInfo[] keyInfos);

        void onRejectPromise(int promiseId,
                             String message);
    } // Callbacks

    public static class NativeMediaDrmProxyCallbacks extends JNIObject implements Callbacks {
        @WrapForJNI(calledFrom = "gecko")
        NativeMediaDrmProxyCallbacks() {}

        @Override
        @WrapForJNI(dispatchTo = "gecko")
        public native void onSessionCreated(int createSessionToken,
                                            int promiseId,
                                            byte[] sessionId,
                                            byte[] request);

        @Override
        @WrapForJNI(dispatchTo = "gecko")
        public native void onSessionUpdated(int promiseId, byte[] sessionId);

        @Override
        @WrapForJNI(dispatchTo = "gecko")
        public native void onSessionClosed(int promiseId, byte[] sessionId);

        @Override
        @WrapForJNI(dispatchTo = "gecko")
        public native void onSessionMessage(byte[] sessionId,
                                            int sessionMessageType,
                                            byte[] request);

        @Override
        @WrapForJNI(dispatchTo = "gecko")
        public native void onSessionError(byte[] sessionId,
                                          String message);

        @Override
        @WrapForJNI(dispatchTo = "gecko")
        public native void onSessionBatchedKeyChanged(byte[] sessionId,
                                                      SessionKeyInfo[] keyInfos);

        @Override
        @WrapForJNI(dispatchTo = "gecko")
        public native void onRejectPromise(int promiseId,
                                           String message);

        @Override // JNIObject
        protected void disposeNative() {
            throw new UnsupportedOperationException();
        }
    } // NativeMediaDrmProxyCallbacks

    // A proxy to callback from LocalMediaDrmBridge to native instance.
    public static class MediaDrmProxyCallbacks implements GeckoMediaDrm.Callbacks {
        private final Callbacks mNativeCallbacks;
        private final MediaDrmProxy mProxy;

        public MediaDrmProxyCallbacks(MediaDrmProxy proxy, Callbacks callbacks) {
            mNativeCallbacks = callbacks;
            mProxy = proxy;
        }

        @Override
        public void onSessionCreated(int createSessionToken,
                                     int promiseId,
                                     byte[] sessionId,
                                     byte[] request) {
            if (!mProxy.isDestroyed()) {
                mNativeCallbacks.onSessionCreated(createSessionToken,
                                                  promiseId,
                                                  sessionId,
                                                  request);
            }
        }

        @Override
        public void onSessionUpdated(int promiseId, byte[] sessionId) {
            if (!mProxy.isDestroyed()) {
                mNativeCallbacks.onSessionUpdated(promiseId, sessionId);
            }
        }

        @Override
        public void onSessionClosed(int promiseId, byte[] sessionId) {
            if (!mProxy.isDestroyed()) {
                mNativeCallbacks.onSessionClosed(promiseId, sessionId);
            }
        }

        @Override
        public void onSessionMessage(byte[] sessionId,
                                     int sessionMessageType,
                                     byte[] request) {
            if (!mProxy.isDestroyed()) {
                mNativeCallbacks.onSessionMessage(sessionId, sessionMessageType, request);
            }
        }

        @Override
        public void onSessionError(byte[] sessionId,
                                   String message) {
            if (!mProxy.isDestroyed()) {
                mNativeCallbacks.onSessionError(sessionId, message);
            }
        }

        @Override
        public void onSessionBatchedKeyChanged(byte[] sessionId,
                                               SessionKeyInfo[] keyInfos) {
            if (!mProxy.isDestroyed()) {
                mNativeCallbacks.onSessionBatchedKeyChanged(sessionId, keyInfos);
            }
        }

        @Override
        public void onRejectPromise(int promiseId,
                                    String message) {
            if (!mProxy.isDestroyed()) {
                mNativeCallbacks.onRejectPromise(promiseId, message);
            }
        }
    } // MediaDrmProxyCallbacks

    public boolean isDestroyed() {
        return mDestroyed;
    }

    @WrapForJNI(calledFrom = "gecko")
    public static MediaDrmProxy create(String keySystem,
                                       Callbacks nativeCallbacks,
                                       boolean isRemote) {
        MediaDrmProxy proxy = new MediaDrmProxy(keySystem, nativeCallbacks, isRemote);
        return proxy;
    }

    MediaDrmProxy(String keySystem, Callbacks nativeCallbacks, boolean isRemote) {
        if (DEBUG) Log.d(LOGTAG, "Constructing MediaDrmProxy");
        try {
            mDrmStubId = UUID.randomUUID().toString();
            if (isRemote) {
                IMediaDrmBridge remoteBridge =
                    RemoteManager.getInstance().createRemoteMediaDrmBridge(keySystem, mDrmStubId);
                mImpl = new RemoteMediaDrmBridge(remoteBridge);
            } else {
                mImpl = new LocalMediaDrmBridge(keySystem);
            }
            mImpl.setCallbacks(new MediaDrmProxyCallbacks(this, nativeCallbacks));
            sProxyList.add(this);
        } catch (Exception e) {
            Log.e(LOGTAG, "Constructing MediaDrmProxy ... error", e);
        }
    }

    @WrapForJNI
    private void createSession(int createSessionToken,
                               int promiseId,
                               String initDataType,
                               byte[] initData) {
        if (DEBUG) Log.d(LOGTAG, "createSession, promiseId = " + promiseId);
        mImpl.createSession(createSessionToken,
                            promiseId,
                            initDataType,
                            initData);
    }

    @WrapForJNI
    private void updateSession(int promiseId, String sessionId, byte[] response) {
        if (DEBUG) Log.d(LOGTAG, "updateSession, primiseId(" + promiseId  + "sessionId(" + sessionId + ")");
        mImpl.updateSession(promiseId, sessionId, response);
    }

    @WrapForJNI
    private void closeSession(int promiseId, String sessionId) {
        if (DEBUG) Log.d(LOGTAG, "closeSession, primiseId(" + promiseId  + "sessionId(" + sessionId + ")");
        mImpl.closeSession(promiseId, sessionId);
    }

    @WrapForJNI(calledFrom = "gecko")
    private String getStubId() {
        return mDrmStubId;
    }

    // Get corresponding MediaCrypto object by a generated UUID for MediaCodec.
    // Will be called on MediaFormatReader's TaskQueue.
    @WrapForJNI
    public static MediaCrypto getMediaCrypto(String stubId) {
        for (MediaDrmProxy proxy : sProxyList) {
            if (proxy.getStubId().equals(stubId)) {
                return proxy.getMediaCryptoFromBridge();
            }
        }
        if (DEBUG) Log.d(LOGTAG, " NULL crytpo ");
        return null;
    }

    @WrapForJNI // Called when natvie object is destroyed.
    private void destroy() {
        if (DEBUG) Log.d(LOGTAG, "destroy!! Native object is destroyed.");
        if (mDestroyed) {
            return;
        }
        mDestroyed = true;
        release();
    }

    private void release() {
        if (DEBUG) Log.d(LOGTAG, "release");
        sProxyList.remove(this);
        mImpl.release();
    }

    private MediaCrypto getMediaCryptoFromBridge() {
        return mImpl != null ? mImpl.getMediaCrypto() : null;
    }
}
