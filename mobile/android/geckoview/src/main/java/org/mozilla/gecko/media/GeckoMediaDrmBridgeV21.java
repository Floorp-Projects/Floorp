/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.annotation.SuppressLint;
import android.media.DeniedByServerException;
import android.media.MediaCrypto;
import android.media.MediaDrm;
import android.media.NotProvisionedException;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URLEncoder;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.util.ArrayDeque;
import java.util.HashMap;
import java.util.HashSet;
import java.util.UUID;
import org.mozilla.gecko.util.ProxySelector;

public class GeckoMediaDrmBridgeV21 implements GeckoMediaDrm {
  protected final String LOGTAG;
  private static final String INVALID_SESSION_ID = "Invalid";
  private static final String WIDEVINE_KEY_SYSTEM = "com.widevine.alpha";
  private static final boolean DEBUG = false;
  private static final UUID WIDEVINE_SCHEME_UUID =
      new UUID(0xedef8ba979d64aceL, 0xa3c827dcd51d21edL);
  private static final int MAX_PROMISE_ID = Integer.MAX_VALUE;
  // MediaDrm.KeyStatus information listener is supported on M+, adding a
  // dummy key id to report key status.
  private static final byte[] DUMMY_KEY_ID = new byte[] {0};

  public static final Charset UTF_8 = Charset.forName("UTF-8");

  private UUID mSchemeUUID;
  private Handler mHandler;
  PostRequestTask mProvisionTask;
  private HandlerThread mHandlerThread;
  private ByteBuffer mCryptoSessionId;

  // mProvisioningPromiseId is great than 0 only during provisioning.
  private int mProvisioningPromiseId;
  private HashSet<ByteBuffer> mSessionIds;
  private HashMap<ByteBuffer, String> mSessionMIMETypes;
  private ArrayDeque<PendingCreateSessionData> mPendingCreateSessionDataQueue;
  private PendingKeyRequest mPendingKeyRequest;
  private GeckoMediaDrm.Callbacks mCallbacks;

  private MediaCrypto mCrypto;
  protected MediaDrm mDrm;

  public static final int LICENSE_REQUEST_INITIAL = 0; /*MediaKeyMessageType::License_request*/
  public static final int LICENSE_REQUEST_RENEWAL = 1; /*MediaKeyMessageType::License_renewal*/
  public static final int LICENSE_REQUEST_RELEASE = 2; /*MediaKeyMessageType::License_release*/

  // Store session data while provisioning
  private static class PendingCreateSessionData {
    public final int mToken;
    public final int mPromiseId;
    public final byte[] mInitData;
    public final String mMimeType;

    private PendingCreateSessionData(
        final int token, final int promiseId, final byte[] initData, final String mimeType) {
      mToken = token;
      mPromiseId = promiseId;
      mInitData = initData;
      mMimeType = mimeType;
    }
  }

  private static class PendingKeyRequest {
    public final ByteBuffer mSession;
    public final byte[] mData;
    public final String mMimeType;

    private PendingKeyRequest(final ByteBuffer session, final byte[] data, final String mimeType) {
      mSession = session;
      mData = data;
      mMimeType = mimeType;
    }
  }

  public boolean isSecureDecoderComonentRequired(final String mimeType) {
    if (mCrypto != null) {
      return mCrypto.requiresSecureDecoderComponent(mimeType);
    }
    return false;
  }

  private static void assertTrue(final boolean condition) {
    if (DEBUG && !condition) {
      throw new AssertionError("Expected condition to be true");
    }
  }

  @SuppressLint("WrongConstant")
  private void configureVendorSpecificProperty() {
    assertTrue(mDrm != null);
    if (mDrm == null) {
      return;
    }
    // Support L3 for now
    mDrm.setPropertyString("securityLevel", "L3");
    // Refer to chromium, set multi-session mode for Widevine.
    if (mSchemeUUID.equals(WIDEVINE_SCHEME_UUID)) {
      mDrm.setPropertyString("privacyMode", "enable");
      mDrm.setPropertyString("sessionSharing", "enable");
    }
  }

  GeckoMediaDrmBridgeV21(final String keySystem) throws Exception {
    LOGTAG = getClass().getSimpleName();
    if (DEBUG) Log.d(LOGTAG, "GeckoMediaDrmBridgeV21 ctor");

    mProvisioningPromiseId = 0;
    mSessionIds = new HashSet<ByteBuffer>();
    mSessionMIMETypes = new HashMap<ByteBuffer, String>();
    mPendingCreateSessionDataQueue = new ArrayDeque<PendingCreateSessionData>();

    mSchemeUUID = convertKeySystemToSchemeUUID(keySystem);
    mCryptoSessionId = null;

    if (DEBUG) Log.d(LOGTAG, "mSchemeUUID : " + mSchemeUUID.toString());

    // The caller of GeckoMediaDrmBridgeV21 ctor should handle exceptions
    // threw by the following steps.
    mDrm = new MediaDrm(mSchemeUUID);
    configureVendorSpecificProperty();
    mDrm.setOnEventListener(new MediaDrmListener());
    try {
      // ensureMediaCryptoCreated may cause NotProvisionedException for the first time use.
      // Need to start provisioning with a dummy promise id.
      ensureMediaCryptoCreated();
    } catch (final android.media.NotProvisionedException e) {
      if (DEBUG) Log.d(LOGTAG, "Device not provisioned:" + e.getMessage());
      startProvisioning(MAX_PROMISE_ID);
    }
  }

  @Override
  public void setCallbacks(final GeckoMediaDrm.Callbacks callbacks) {
    assertTrue(callbacks != null);
    mCallbacks = callbacks;
  }

  @Override
  public void createSession(
      final int createSessionToken,
      final int promiseId,
      final String initDataType,
      final byte[] initData) {
    if (DEBUG) Log.d(LOGTAG, "createSession()");
    if (mDrm == null) {
      onRejectPromise(promiseId, "MediaDrm instance doesn't exist !!");
      return;
    }

    if (mProvisioningPromiseId > 0 && mCrypto == null) {
      if (DEBUG) Log.d(LOGTAG, "Pending createSession because it's provisioning !");
      savePendingCreateSessionData(
          createSessionToken, promiseId,
          initData, initDataType);
      return;
    }

    ByteBuffer sessionId = null;
    try {
      final boolean hasMediaCrypto = ensureMediaCryptoCreated();
      if (!hasMediaCrypto) {
        onRejectPromise(promiseId, "MediaCrypto intance is not created !");
        return;
      }

      sessionId = openSession();
      if (sessionId == null) {
        onRejectPromise(promiseId, "Cannot get a session id from MediaDrm !");
        return;
      }

      final MediaDrm.KeyRequest request = getKeyRequest(sessionId, initData, initDataType);
      if (request == null) {
        mDrm.closeSession(sessionId.array());
        onRejectPromise(promiseId, "Cannot get a key request from MediaDrm !");
        return;
      }
      onSessionCreated(createSessionToken, promiseId, sessionId.array(), request.getData());
      onSessionMessage(sessionId.array(), LICENSE_REQUEST_INITIAL, request.getData());
      mSessionMIMETypes.put(sessionId, initDataType);
      mSessionIds.add(sessionId);
      if (DEBUG)
        Log.d(
            LOGTAG,
            " StringID : " + new String(sessionId.array(), UTF_8) + " is put into mSessionIds ");
    } catch (final android.media.NotProvisionedException e) {
      if (DEBUG) Log.d(LOGTAG, "Device not provisioned:" + e.getMessage());
      if (sessionId != null) {
        // The promise of this createSession will be either resolved
        // or rejected after provisioning.
        mDrm.closeSession(sessionId.array());
      }
      savePendingCreateSessionData(
          createSessionToken, promiseId,
          initData, initDataType);
      startProvisioning(promiseId);
    }
  }

  @Override
  public void updateSession(final int promiseId, final String sessionId, final byte[] response) {
    if (DEBUG) Log.d(LOGTAG, "updateSession(), sessionId = " + sessionId);
    if (mDrm == null) {
      onRejectPromise(promiseId, "MediaDrm instance doesn't exist !!");
      return;
    }

    final ByteBuffer session = ByteBuffer.wrap(sessionId.getBytes(UTF_8));
    if (!sessionExists(session)) {
      onRejectPromise(promiseId, "Invalid session during updateSession.");
      return;
    }

    try {
      final byte[] keySetId = mDrm.provideKeyResponse(session.array(), response);
      if (DEBUG) {
        final HashMap<String, String> infoMap = mDrm.queryKeyStatus(session.array());
        for (final String strKey : infoMap.keySet()) {
          final String strValue = infoMap.get(strKey);
          Log.d(LOGTAG, "InfoMap : key(" + strKey + ")/value(" + strValue + ")");
        }
      }
      HandleKeyStatusChangeByDummyKey(sessionId);
      onSessionUpdated(promiseId, session.array());
      return;
    } catch (final NotProvisionedException | DeniedByServerException | IllegalStateException e) {
      if (DEBUG) Log.d(LOGTAG, "Failed to provide key response:", e);
      onSessionError(session.array(), "Got exception during updateSession.");
      onRejectPromise(promiseId, "Got exception during updateSession.");
    }
    release();
    return;
  }

  @Override
  public void closeSession(final int promiseId, final String sessionId) {
    if (DEBUG) Log.d(LOGTAG, "closeSession()");
    if (mDrm == null) {
      onRejectPromise(promiseId, "MediaDrm instance doesn't exist !!");
      return;
    }

    final ByteBuffer session = ByteBuffer.wrap(sessionId.getBytes(UTF_8));
    mSessionIds.remove(session);
    mDrm.closeSession(session.array());
    onSessionClosed(promiseId, session.array());
  }

  @Override
  public void release() {
    if (DEBUG) Log.d(LOGTAG, "release()");
    if (mProvisionTask != null) {
      mProvisionTask.cancel(true);
      mProvisionTask = null;
    }
    if (mProvisioningPromiseId > 0) {
      onRejectPromise(mProvisioningPromiseId, "Releasing ... reject provisioning session.");
      mProvisioningPromiseId = 0;
    }
    if (mPendingKeyRequest != null) {
      mPendingKeyRequest = null;
    }
    while (!mPendingCreateSessionDataQueue.isEmpty()) {
      final PendingCreateSessionData pendingData = mPendingCreateSessionDataQueue.poll();
      if (pendingData != null) {
        onRejectPromise(pendingData.mPromiseId, "Releasing ... reject all pending sessions.");
      }
    }
    mPendingCreateSessionDataQueue = null;

    if (mDrm != null) {
      for (final ByteBuffer session : mSessionIds) {
        mDrm.closeSession(session.array());
      }
      mDrm.release();
      mDrm = null;
    }
    mSessionIds.clear();
    mSessionIds = null;
    mSessionMIMETypes.clear();
    mSessionMIMETypes = null;

    mCryptoSessionId = null;
    if (mCrypto != null) {
      mCrypto.release();
      mCrypto = null;
    }
    if (mHandlerThread != null) {
      mHandlerThread.quitSafely();
      mHandlerThread = null;
    }
    mHandler = null;
  }

  @Override
  public MediaCrypto getMediaCrypto() {
    if (DEBUG) Log.d(LOGTAG, "getMediaCrypto()");
    return mCrypto;
  }

  @SuppressLint("WrongConstant")
  @Override
  public void setServerCertificate(final byte[] cert) {
    if (DEBUG) Log.d(LOGTAG, "setServerCertificate()");
    if (mDrm == null) {
      throw new IllegalStateException("MediaDrm instance doesn't exist !!");
    }
    mDrm.setPropertyByteArray("serviceCertificate", cert);
    return;
  }

  protected void HandleKeyStatusChangeByDummyKey(final String sessionId) {
    final SessionKeyInfo[] keyInfos = new SessionKeyInfo[1];
    keyInfos[0] = new SessionKeyInfo(DUMMY_KEY_ID, MediaDrm.KeyStatus.STATUS_USABLE);
    onSessionBatchedKeyChanged(sessionId.getBytes(), keyInfos);
    if (DEBUG) Log.d(LOGTAG, "Key successfully added for session " + sessionId);
  }

  protected void onSessionCreated(
      final int createSessionToken,
      final int promiseId,
      final byte[] sessionId,
      final byte[] request) {
    assertTrue(mCallbacks != null);
    if (mCallbacks != null) {
      mCallbacks.onSessionCreated(createSessionToken, promiseId, sessionId, request);
    }
  }

  protected void onSessionUpdated(final int promiseId, final byte[] sessionId) {
    assertTrue(mCallbacks != null);
    if (mCallbacks != null) {
      mCallbacks.onSessionUpdated(promiseId, sessionId);
    }
  }

  protected void onSessionClosed(final int promiseId, final byte[] sessionId) {
    assertTrue(mCallbacks != null);
    if (mCallbacks != null) {
      mCallbacks.onSessionClosed(promiseId, sessionId);
    }
  }

  protected void onSessionMessage(
      final byte[] sessionId, final int sessionMessageType, final byte[] request) {
    assertTrue(mCallbacks != null);
    if (mCallbacks != null) {
      mCallbacks.onSessionMessage(sessionId, sessionMessageType, request);
    }
  }

  protected void onSessionError(final byte[] sessionId, final String message) {
    assertTrue(mCallbacks != null);
    if (mCallbacks != null) {
      mCallbacks.onSessionError(sessionId, message);
    }
  }

  protected void onSessionBatchedKeyChanged(
      final byte[] sessionId, final SessionKeyInfo[] keyInfos) {
    assertTrue(mCallbacks != null);
    if (mCallbacks != null) {
      mCallbacks.onSessionBatchedKeyChanged(sessionId, keyInfos);
    }
  }

  protected void onRejectPromise(final int promiseId, final String message) {
    assertTrue(mCallbacks != null);
    if (mCallbacks != null) {
      mCallbacks.onRejectPromise(promiseId, message);
    }
  }

  private MediaDrm.KeyRequest getKeyRequest(
      final ByteBuffer aSession, final byte[] data, final String mimeType)
      throws android.media.NotProvisionedException {
    if (mProvisioningPromiseId > 0) {
      if (DEBUG) Log.d(LOGTAG, "Now provisioning");
      return null;
    }

    try {
      final HashMap<String, String> optionalParameters = new HashMap<String, String>();
      return mDrm.getKeyRequest(
          aSession.array(), data, mimeType, MediaDrm.KEY_TYPE_STREAMING, optionalParameters);
    } catch (final Exception e) {
      Log.e(LOGTAG, "Got excpetion during MediaDrm.getKeyRequest", e);
    }
    return null;
  }

  private class MediaDrmListener implements MediaDrm.OnEventListener {
    @Override
    public void onEvent(
        final MediaDrm mediaDrm,
        final byte[] sessionArray,
        final int event,
        final int extra,
        final byte[] data) {
      if (DEBUG) Log.d(LOGTAG, "MediaDrmListener.onEvent()");
      if (sessionArray == null) {
        if (DEBUG) Log.d(LOGTAG, "MediaDrmListener: Null session.");
        return;
      }
      final ByteBuffer session = ByteBuffer.wrap(sessionArray);
      if (!sessionExists(session)) {
        if (DEBUG) Log.d(LOGTAG, "MediaDrmListener: Invalid session.");
        return;
      }
      // On L, these events are treated as exceptions and handled correspondingly.
      // Leaving this code block for logging message.
      switch (event) {
        case MediaDrm.EVENT_PROVISION_REQUIRED:
          if (DEBUG) Log.d(LOGTAG, "MediaDrm.EVENT_PROVISION_REQUIRED");
          break;
        case MediaDrm.EVENT_KEY_REQUIRED:
          if (DEBUG)
            Log.d(
                LOGTAG,
                "MediaDrm.EVENT_KEY_REQUIRED, sessionId=" + new String(session.array(), UTF_8));
          final String mimeType = mSessionMIMETypes.get(session);
          MediaDrm.KeyRequest request = null;
          try {
            request = getKeyRequest(session, data, mimeType);
          } catch (final android.media.NotProvisionedException e) {
            Log.w(LOGTAG, "MediaDrm.EVENT_KEY_REQUIRED, Device not provisioned.", e);
            startProvisioning(MAX_PROMISE_ID);
            mPendingKeyRequest = new PendingKeyRequest(session, data, mimeType);
            return;
          }
          requestLicense(sessionArray, request);
          break;
        case MediaDrm.EVENT_KEY_EXPIRED:
          if (DEBUG)
            Log.d(
                LOGTAG,
                "MediaDrm.EVENT_KEY_EXPIRED, sessionId=" + new String(session.array(), UTF_8));
          break;
        case MediaDrm.EVENT_VENDOR_DEFINED:
          if (DEBUG)
            Log.d(
                LOGTAG,
                "MediaDrm.EVENT_VENDOR_DEFINED, sessionId=" + new String(session.array(), UTF_8));
          break;
        case MediaDrm.EVENT_SESSION_RECLAIMED:
          if (DEBUG)
            Log.d(
                LOGTAG,
                "MediaDrm.EVENT_SESSION_RECLAIMED, sessionId="
                    + new String(session.array(), UTF_8));
          break;
        default:
          if (DEBUG) Log.d(LOGTAG, "Invalid DRM event " + event);
          return;
      }
    }
  }

  private ByteBuffer openSession() throws android.media.NotProvisionedException {
    try {
      final byte[] sessionId = mDrm.openSession();
      // ByteBuffer.wrap() is backed by the byte[]. Make a clone here in
      // case the underlying byte[] is modified.
      return ByteBuffer.wrap(sessionId.clone());
    } catch (final android.media.NotProvisionedException e) {
      // Throw NotProvisionedException so that we can startProvisioning().
      throw e;
    } catch (final java.lang.RuntimeException e) {
      if (DEBUG) Log.d(LOGTAG, "Cannot open a new session:" + e.getMessage());
      release();
      return null;
    } catch (final android.media.MediaDrmException e) {
      // Other MediaDrmExceptions (e.g. ResourceBusyException) are not
      // recoverable.
      release();
      return null;
    }
  }

  protected boolean sessionExists(final ByteBuffer session) {
    if (mCryptoSessionId == null) {
      if (DEBUG)
        Log.d(LOGTAG, "Session doesn't exist because media crypto session is not created.");
      return false;
    }
    if (session == null) {
      if (DEBUG) Log.d(LOGTAG, "Session is null, not in map !");
      return false;
    }
    return !session.equals(mCryptoSessionId) && mSessionIds.contains(session);
  }

  private class PostRequestTask extends AsyncTask<Void, Void, Void> {
    private static final String LOGTAG = "PostRequestTask";

    private int mPromiseId;
    private String mURL;
    private byte[] mDrmRequest;
    private byte[] mResponseBody;

    PostRequestTask(final int promiseId, final String url, final byte[] drmRequest) {
      this.mPromiseId = promiseId;
      this.mURL = url;
      this.mDrmRequest = drmRequest;
    }

    @Override
    protected Void doInBackground(final Void... params) {
      HttpURLConnection urlConnection = null;
      BufferedReader in = null;
      try {
        final URI finalURI =
            new URI(mURL + "&signedRequest=" + URLEncoder.encode(new String(mDrmRequest), "UTF-8"));
        urlConnection = (HttpURLConnection) ProxySelector.openConnectionWithProxy(finalURI);
        urlConnection.setRequestMethod("POST");
        if (DEBUG) Log.d(LOGTAG, "Provisioning, posting url =" + finalURI.toString());

        // Add data
        urlConnection.setRequestProperty("Accept", "*/*");
        urlConnection.setRequestProperty("User-Agent", getCDMUserAgent());
        urlConnection.setRequestProperty("Content-Type", "application/json");

        // Execute HTTP Post Request
        urlConnection.connect();

        final int responseCode = urlConnection.getResponseCode();
        if (responseCode == HttpURLConnection.HTTP_OK) {
          in = new BufferedReader(new InputStreamReader(urlConnection.getInputStream(), UTF_8));
          String inputLine;
          final StringBuffer response = new StringBuffer();

          while ((inputLine = in.readLine()) != null) {
            response.append(inputLine);
          }
          in.close();
          mResponseBody = String.valueOf(response).getBytes(UTF_8);
          if (DEBUG) Log.d(LOGTAG, "Provisioning, response received.");
          if (mResponseBody != null) Log.d(LOGTAG, "response length=" + mResponseBody.length);
        } else {
          Log.d(LOGTAG, "Provisioning, server returned HTTP error code :" + responseCode);
        }
      } catch (final IOException e) {
        Log.e(LOGTAG, "Got exception during posting provisioning request ...", e);
      } catch (final URISyntaxException e) {
        Log.e(LOGTAG, "Got exception during creating uri ...", e);
      } finally {
        if (urlConnection != null) {
          urlConnection.disconnect();
        }
        try {
          if (in != null) {
            in.close();
          }
        } catch (final IOException e) {
          Log.e(LOGTAG, "Exception during closing in ...", e);
        }
      }
      return null;
    }

    @Override
    protected void onPostExecute(final Void v) {
      onProvisionResponse(mPromiseId, mResponseBody);
    }
  }

  private boolean provideProvisionResponse(final byte[] response) {
    if (response == null || response.length == 0) {
      if (DEBUG) Log.d(LOGTAG, "Invalid provision response.");
      return false;
    }

    try {
      mDrm.provideProvisionResponse(response);
      return true;
    } catch (final android.media.DeniedByServerException e) {
      if (DEBUG) Log.d(LOGTAG, "Failed to provide provision response:" + e.getMessage());
    } catch (final java.lang.IllegalStateException e) {
      if (DEBUG) Log.d(LOGTAG, "Failed to provide provision response:" + e.getMessage());
    }
    return false;
  }

  private void savePendingCreateSessionData(
      final int token, final int promiseId, final byte[] initData, final String mime) {
    if (DEBUG) Log.d(LOGTAG, "savePendingCreateSessionData, promiseId : " + promiseId);
    mPendingCreateSessionDataQueue.offer(
        new PendingCreateSessionData(token, promiseId, initData, mime));
  }

  private void processPendingCreateSessionData() {
    if (DEBUG) Log.d(LOGTAG, "processPendingCreateSessionData ... ");

    assertTrue(mProvisioningPromiseId == 0);
    try {
      while (!mPendingCreateSessionDataQueue.isEmpty()) {
        final PendingCreateSessionData pendingData = mPendingCreateSessionDataQueue.poll();
        if (pendingData == null) {
          return;
        }
        if (DEBUG)
          Log.d(LOGTAG, "processPendingCreateSessionData, promiseId : " + pendingData.mPromiseId);

        createSession(
            pendingData.mToken,
            pendingData.mPromiseId,
            pendingData.mMimeType,
            pendingData.mInitData);
      }
    } catch (final Exception e) {
      Log.e(LOGTAG, "Got excpetion during processPendingCreateSessionData ...", e);
    }
  }

  private void resumePendingOperations() {
    if (mHandlerThread == null) {
      mHandlerThread = new HandlerThread("PendingSessionOpsThread");
      mHandlerThread.start();
    }
    if (mHandler == null) {
      mHandler = new Handler(mHandlerThread.getLooper());
    }
    mHandler.post(
        new Runnable() {
          @Override
          public void run() {
            if (mPendingKeyRequest != null) {
              MediaDrm.KeyRequest request = null;
              try {
                request =
                    getKeyRequest(
                        mPendingKeyRequest.mSession,
                        mPendingKeyRequest.mData,
                        mPendingKeyRequest.mMimeType);
              } catch (final NotProvisionedException e) {
                Log.e(LOGTAG, "Cannot get key request after provisioning!");
                return;
              } finally {
                mPendingKeyRequest = null;
              }
              requestLicense(mPendingKeyRequest.mSession.array(), request);
            } else {
              processPendingCreateSessionData();
            }
          }
        });
  }

  private void requestLicense(final byte[] session, final MediaDrm.KeyRequest request) {
    if (request == null) {
      Log.e(LOGTAG, "null key request when requesting license");
      return;
    }
    // The EME spec says the messageType is only for optimization and optional.
    // Send 'License_request' as default when it's not available.
    int requestType = LICENSE_REQUEST_INITIAL;
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
      requestType = request.getRequestType();
    }
    onSessionMessage(session, requestType, request.getData());
  }

  // Only triggered when failed on {openSession, getKeyRequest}
  private void startProvisioning(final int promiseId) {
    if (DEBUG) Log.d(LOGTAG, "startProvisioning()");
    if (mProvisioningPromiseId > 0) {
      // Already in provisioning.
      return;
    }
    try {
      mProvisioningPromiseId = promiseId;
      final MediaDrm.ProvisionRequest request = mDrm.getProvisionRequest();
      mProvisionTask = new PostRequestTask(promiseId, request.getDefaultUrl(), request.getData());
      mProvisionTask.execute();
    } catch (final Exception e) {
      onRejectPromise(promiseId, "Exception happened in startProvisioning !");
      mProvisioningPromiseId = 0;
    }
  }

  private void onProvisionResponse(final int promiseId, final byte[] response) {
    if (DEBUG) Log.d(LOGTAG, "onProvisionResponse()");
    mProvisionTask = null;
    mProvisioningPromiseId = 0;
    final boolean success = provideProvisionResponse(response);
    if (success) {
      // Promise will either be resovled / rejected in createSession during
      // resuming operations.
      resumePendingOperations();
    } else {
      onRejectPromise(promiseId, "Failed to provide provision response.");
    }
  }

  private boolean ensureMediaCryptoCreated() throws android.media.NotProvisionedException {
    if (mCrypto != null) {
      return true;
    }
    try {
      mCryptoSessionId = openSession();
      if (mCryptoSessionId == null) {
        if (DEBUG) Log.d(LOGTAG, "Cannot open session for MediaCrypto");
        return false;
      }

      if (MediaCrypto.isCryptoSchemeSupported(mSchemeUUID)) {
        final byte[] cryptoSessionId = mCryptoSessionId.array();
        mCrypto = new MediaCrypto(mSchemeUUID, cryptoSessionId);
        mSessionIds.add(mCryptoSessionId);
        if (DEBUG)
          Log.d(
              LOGTAG,
              "MediaCrypto successfully created! - SId "
                  + INVALID_SESSION_ID
                  + ", "
                  + new String(cryptoSessionId, UTF_8));
        return true;
      } else {
        if (DEBUG) Log.d(LOGTAG, "Cannot create MediaCrypto for unsupported scheme.");
        return false;
      }
    } catch (final android.media.MediaCryptoException e) {
      if (DEBUG) Log.d(LOGTAG, "Cannot create MediaCrypto:" + e.getMessage());
      release();
      return false;
    } catch (final android.media.NotProvisionedException e) {
      if (DEBUG)
        Log.d(LOGTAG, "ensureMediaCryptoCreated::Device not provisioned:" + e.getMessage());
      throw e;
    }
  }

  private UUID convertKeySystemToSchemeUUID(final String keySystem) {
    if (WIDEVINE_KEY_SYSTEM.equals(keySystem)) {
      return WIDEVINE_SCHEME_UUID;
    }
    if (DEBUG) Log.d(LOGTAG, "Cannot convert unsupported key system : " + keySystem);
    return new UUID(0L, 0L);
  }

  private String getCDMUserAgent() {
    // This user agent is found and hard-coded in Android(L) source code and
    // Chromium project. Not sure if it's gonna change in the future.
    return "Widevine CDM v1.0";
  }
}
