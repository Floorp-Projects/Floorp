/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.media.MediaCrypto;
import android.util.Log;

final class RemoteMediaDrmBridge implements GeckoMediaDrm {
    private static final String LOGTAG = "RemoteMediaDrmBridge";
    private static final boolean DEBUG = false;
    private CallbacksForwarder mCallbacksFwd;
    private IMediaDrmBridge mRemote;

    // Forward callbacks from remote bridge stub to MediaDrmProxy.
    private static class CallbacksForwarder extends IMediaDrmBridgeCallbacks.Stub {
        private final GeckoMediaDrm.Callbacks mProxyCallbacks;
        CallbacksForwarder(final Callbacks callbacks) {
            assertTrue(callbacks != null);
            mProxyCallbacks = callbacks;
        }

        @Override
        public void onSessionCreated(final int createSessionToken,
                                     final int promiseId,
                                     final byte[] sessionId,
                                     final byte[] request) {
            mProxyCallbacks.onSessionCreated(createSessionToken,
                                             promiseId,
                                             sessionId,
                                             request);
        }

        @Override
        public void onSessionUpdated(final int promiseId, final byte[] sessionId) {
            mProxyCallbacks.onSessionUpdated(promiseId, sessionId);
        }

        @Override
        public void onSessionClosed(final int promiseId, final byte[] sessionId) {
            mProxyCallbacks.onSessionClosed(promiseId, sessionId);
        }

        @Override
        public void onSessionMessage(final byte[] sessionId,
                                     final int sessionMessageType,
                                     final byte[] request) {
            mProxyCallbacks.onSessionMessage(sessionId, sessionMessageType, request);
        }

        @Override
        public void onSessionError(final byte[] sessionId, final String message) {
            mProxyCallbacks.onSessionError(sessionId, message);
        }

        @Override
        public void onSessionBatchedKeyChanged(final byte[] sessionId,
                                               final SessionKeyInfo[] keyInfos) {
            mProxyCallbacks.onSessionBatchedKeyChanged(sessionId, keyInfos);
        }

        @Override
        public void onRejectPromise(final int promiseId, final String message) {
            mProxyCallbacks.onRejectPromise(promiseId, message);
        }
    } // CallbacksForwarder

    /* package-private */ static void assertTrue(final boolean condition) {
        if (DEBUG && !condition) {
            throw new AssertionError("Expected condition to be true");
        }
    }

    public RemoteMediaDrmBridge(final IMediaDrmBridge remoteBridge) {
        assertTrue(remoteBridge != null);
        mRemote = remoteBridge;
    }

    @Override
    public synchronized void setCallbacks(final Callbacks callbacks) {
        if (DEBUG) Log.d(LOGTAG, "setCallbacks()");
        assertTrue(callbacks != null);
        assertTrue(mRemote != null);

        mCallbacksFwd = new CallbacksForwarder(callbacks);
        try {
            mRemote.setCallbacks(mCallbacksFwd);
        } catch (Exception e) {
            Log.e(LOGTAG, "Got exception during setCallbacks", e);
        }
    }

    @Override
    public synchronized void createSession(final int createSessionToken,
                                           final int promiseId,
                                           final String initDataType,
                                           final byte[] initData) {
        if (DEBUG) Log.d(LOGTAG, "createSession()");

        try {
            mRemote.createSession(createSessionToken, promiseId, initDataType, initData);
        } catch (Exception e) {
            Log.e(LOGTAG, "Got exception while creating remote session.", e);
            mCallbacksFwd.onRejectPromise(promiseId, "Failed to create session.");
        }
    }

    @Override
    public synchronized void updateSession(final int promiseId, final String sessionId,
                                           final byte[] response) {
        if (DEBUG) Log.d(LOGTAG, "updateSession()");

        try {
            mRemote.updateSession(promiseId, sessionId, response);
        } catch (Exception e) {
            Log.e(LOGTAG, "Got exception while updating remote session.", e);
            mCallbacksFwd.onRejectPromise(promiseId, "Failed to update session.");
        }
    }

    @Override
    public synchronized void closeSession(final int promiseId, final String sessionId) {
        if (DEBUG) Log.d(LOGTAG, "closeSession()");

        try {
            mRemote.closeSession(promiseId, sessionId);
        } catch (Exception e) {
            Log.e(LOGTAG, "Got exception while closing remote session.", e);
            mCallbacksFwd.onRejectPromise(promiseId, "Failed to close session.");
        }
    }

    @Override
    public synchronized void release() {
        if (DEBUG) Log.d(LOGTAG, "release()");

        try {
            mRemote.release();
        } catch (Exception e) {
            Log.e(LOGTAG, "Got exception while releasing RemoteDrmBridge.", e);
        }
        RemoteManager.getInstance().onRemoteMediaDrmBridgeReleased(mRemote);
        mRemote = null;
        mCallbacksFwd = null;
    }

    @Override
    public synchronized MediaCrypto getMediaCrypto() {
        if (DEBUG) Log.d(LOGTAG, "getMediaCrypto(), should not enter here!");
        assertTrue(false);
        return null;
    }

    @Override
    public synchronized void setServerCertificate(final byte[] cert) {
        try {
            mRemote.setServerCertificate(cert);
        } catch (Exception e) {
            Log.e(LOGTAG, "Got exception while setting server certificate.", e);
            throw new RuntimeException(e);
        }
    }
}
