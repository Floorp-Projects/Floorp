/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.annotation.SuppressLint;
import android.media.MediaCrypto;
import android.media.MediaDrm;
import android.os.Build;
import android.util.Log;
import java.util.ArrayList;
import java.util.UUID;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.mozglue.JNIObject;

public final class MediaDrmProxy {
  private static final String LOGTAG = "GeckoMediaDrmProxy";
  private static final boolean DEBUG = false;
  private static final UUID WIDEVINE_SCHEME_UUID =
      new UUID(0xedef8ba979d64aceL, 0xa3c827dcd51d21edL);

  private static final String WIDEVINE_KEY_SYSTEM = "com.widevine.alpha";
  @WrapForJNI private static final String AAC = "audio/mp4a-latm";
  @WrapForJNI private static final String AVC = "video/avc";
  @WrapForJNI private static final String VORBIS = "audio/vorbis";
  @WrapForJNI private static final String VP8 = "video/x-vnd.on2.vp8";
  @WrapForJNI private static final String VP9 = "video/x-vnd.on2.vp9";
  @WrapForJNI private static final String OPUS = "audio/opus";
  @WrapForJNI private static final String FLAC = "audio/flac";

  public static final ArrayList<MediaDrmProxy> sProxyList = new ArrayList<MediaDrmProxy>();

  // A flag to avoid using the native object that has been destroyed.
  private boolean mDestroyed;
  private GeckoMediaDrm mImpl;
  private String mDrmStubId;

  private static boolean isSystemSupported() {
    // Support versions >= Marshmallow
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
      if (DEBUG)
        Log.d(LOGTAG, "System Not supported !!, current SDK version is " + Build.VERSION.SDK_INT);
      return false;
    }
    return true;
  }

  @SuppressLint("NewApi")
  @WrapForJNI
  public static boolean isSchemeSupported(final String keySystem) {
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

  @SuppressLint("NewApi")
  @WrapForJNI
  public static boolean IsCryptoSchemeSupported(final String keySystem, final String container) {
    if (!isSystemSupported()) {
      return false;
    }
    if (keySystem.equals(WIDEVINE_KEY_SYSTEM)) {
      return MediaDrm.isCryptoSchemeSupported(WIDEVINE_SCHEME_UUID, container);
    }
    if (DEBUG)
      Log.d(LOGTAG, "cannot decrypt key sytem = " + keySystem + ", container = " + container);
    return false;
  }

  // Interface for callback to native.
  public interface Callbacks {
    void onSessionCreated(int createSessionToken, int promiseId, byte[] sessionId, byte[] request);

    void onSessionUpdated(int promiseId, byte[] sessionId);

    void onSessionClosed(int promiseId, byte[] sessionId);

    void onSessionMessage(byte[] sessionId, int sessionMessageType, byte[] request);

    void onSessionError(byte[] sessionId, String message);

    // MediaDrm.KeyStatus is available in API level 23(M)
    // https://developer.android.com/reference/android/media/MediaDrm.KeyStatus.html
    // For compatibility between L and M above, we'll unwrap the KeyStatus structure
    // and store the keyid and status into SessionKeyInfo and pass to native(MediaDrmCDMProxy).
    void onSessionBatchedKeyChanged(byte[] sessionId, SessionKeyInfo[] keyInfos);

    void onRejectPromise(int promiseId, String message);
  } // Callbacks

  public static class NativeMediaDrmProxyCallbacks extends JNIObject implements Callbacks {
    @WrapForJNI(calledFrom = "gecko")
    NativeMediaDrmProxyCallbacks() {}

    @Override
    @WrapForJNI(dispatchTo = "gecko")
    public native void onSessionCreated(
        int createSessionToken, int promiseId, byte[] sessionId, byte[] request);

    @Override
    @WrapForJNI(dispatchTo = "gecko")
    public native void onSessionUpdated(int promiseId, byte[] sessionId);

    @Override
    @WrapForJNI(dispatchTo = "gecko")
    public native void onSessionClosed(int promiseId, byte[] sessionId);

    @Override
    @WrapForJNI(dispatchTo = "gecko")
    public native void onSessionMessage(byte[] sessionId, int sessionMessageType, byte[] request);

    @Override
    @WrapForJNI(dispatchTo = "gecko")
    public native void onSessionError(byte[] sessionId, String message);

    @Override
    @WrapForJNI(dispatchTo = "gecko")
    public native void onSessionBatchedKeyChanged(byte[] sessionId, SessionKeyInfo[] keyInfos);

    @Override
    @WrapForJNI(dispatchTo = "gecko")
    public native void onRejectPromise(int promiseId, String message);

    @Override // JNIObject
    protected void disposeNative() {
      throw new UnsupportedOperationException();
    }
  } // NativeMediaDrmProxyCallbacks

  // A proxy to callback from LocalMediaDrmBridge to native instance.
  public static class MediaDrmProxyCallbacks implements GeckoMediaDrm.Callbacks {
    private final Callbacks mNativeCallbacks;
    private final MediaDrmProxy mProxy;

    public MediaDrmProxyCallbacks(final MediaDrmProxy proxy, final Callbacks callbacks) {
      mNativeCallbacks = callbacks;
      mProxy = proxy;
    }

    @Override
    public void onSessionCreated(
        final int createSessionToken,
        final int promiseId,
        final byte[] sessionId,
        final byte[] request) {
      if (!mProxy.isDestroyed()) {
        mNativeCallbacks.onSessionCreated(createSessionToken, promiseId, sessionId, request);
      }
    }

    @Override
    public void onSessionUpdated(final int promiseId, final byte[] sessionId) {
      if (!mProxy.isDestroyed()) {
        mNativeCallbacks.onSessionUpdated(promiseId, sessionId);
      }
    }

    @Override
    public void onSessionClosed(final int promiseId, final byte[] sessionId) {
      if (!mProxy.isDestroyed()) {
        mNativeCallbacks.onSessionClosed(promiseId, sessionId);
      }
    }

    @Override
    public void onSessionMessage(
        final byte[] sessionId, final int sessionMessageType, final byte[] request) {
      if (!mProxy.isDestroyed()) {
        mNativeCallbacks.onSessionMessage(sessionId, sessionMessageType, request);
      }
    }

    @Override
    public void onSessionError(final byte[] sessionId, final String message) {
      if (!mProxy.isDestroyed()) {
        mNativeCallbacks.onSessionError(sessionId, message);
      }
    }

    @Override
    public void onSessionBatchedKeyChanged(
        final byte[] sessionId, final SessionKeyInfo[] keyInfos) {
      if (!mProxy.isDestroyed()) {
        mNativeCallbacks.onSessionBatchedKeyChanged(sessionId, keyInfos);
      }
    }

    @Override
    public void onRejectPromise(final int promiseId, final String message) {
      if (!mProxy.isDestroyed()) {
        mNativeCallbacks.onRejectPromise(promiseId, message);
      }
    }
  } // MediaDrmProxyCallbacks

  public boolean isDestroyed() {
    return mDestroyed;
  }

  @WrapForJNI(calledFrom = "gecko")
  public static MediaDrmProxy create(final String keySystem, final Callbacks nativeCallbacks) {
    return new MediaDrmProxy(keySystem, nativeCallbacks);
  }

  MediaDrmProxy(final String keySystem, final Callbacks nativeCallbacks) {
    if (DEBUG) Log.d(LOGTAG, "Constructing MediaDrmProxy");
    try {
      mDrmStubId = UUID.randomUUID().toString();
      final IMediaDrmBridge remoteBridge =
          RemoteManager.getInstance().createRemoteMediaDrmBridge(keySystem, mDrmStubId);
      mImpl = new RemoteMediaDrmBridge(remoteBridge);
      mImpl.setCallbacks(new MediaDrmProxyCallbacks(this, nativeCallbacks));
      sProxyList.add(this);
    } catch (final Exception e) {
      Log.e(LOGTAG, "Constructing MediaDrmProxy ... error", e);
    }
  }

  @WrapForJNI
  private void createSession(
      final int createSessionToken,
      final int promiseId,
      final String initDataType,
      final byte[] initData) {
    if (DEBUG) Log.d(LOGTAG, "createSession, promiseId = " + promiseId);
    mImpl.createSession(createSessionToken, promiseId, initDataType, initData);
  }

  @WrapForJNI
  private void updateSession(final int promiseId, final String sessionId, final byte[] response) {
    if (DEBUG)
      Log.d(LOGTAG, "updateSession, primiseId(" + promiseId + "sessionId(" + sessionId + ")");
    mImpl.updateSession(promiseId, sessionId, response);
  }

  @WrapForJNI
  private void closeSession(final int promiseId, final String sessionId) {
    if (DEBUG)
      Log.d(LOGTAG, "closeSession, primiseId(" + promiseId + "sessionId(" + sessionId + ")");
    mImpl.closeSession(promiseId, sessionId);
  }

  @WrapForJNI(calledFrom = "gecko")
  private String getStubId() {
    return mDrmStubId;
  }

  @WrapForJNI
  public boolean setServerCertificate(final byte[] cert) {
    try {
      mImpl.setServerCertificate(cert);
      return true;
    } catch (final RuntimeException e) {
      return false;
    }
  }

  // Get corresponding MediaCrypto object by a generated UUID for MediaCodec.
  // Will be called on MediaFormatReader's TaskQueue.
  @WrapForJNI
  public static MediaCrypto getMediaCrypto(final String stubId) {
    for (final MediaDrmProxy proxy : sProxyList) {
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
