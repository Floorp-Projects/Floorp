/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.media.MediaCrypto;
import android.util.Log;

final class RemoteMediaDrmBridge implements GeckoMediaDrm {
    private static final String LOGTAG = "GeckoRemoteMediaDrmBridge";
    private static final boolean DEBUG = false;
    private CallbacksForwarder mCallbacksFwd;
    private IMediaDrmBridge mRemote;

    // Forward callbacks from remote bridge stub to MediaDrmProxy.
    private static class CallbacksForwarder extends IMediaDrmBridgeCallbacks.Stub {
        private final GeckoMediaDrm.Callbacks mProxyCallbacks;
        CallbacksForwarder(Callbacks callbacks) {
            assertTrue(callbacks != null);
            mProxyCallbacks = callbacks;
        }

        @Override
        public void onSessionCreated(int createSessionToken,
                                     int promiseId,
                                     byte[] sessionId,
                                     byte[] request) {
            mProxyCallbacks.onSessionCreated(createSessionToken,
                                             promiseId,
                                             sessionId,
                                             request);
        }

        @Override
        public void onSessionUpdated(int promiseId, byte[] sessionId) {
            mProxyCallbacks.onSessionUpdated(promiseId, sessionId);
        }

        @Override
        public void onSessionClosed(int promiseId, byte[] sessionId) {
            mProxyCallbacks.onSessionClosed(promiseId, sessionId);
        }

        @Override
        public void onSessionMessage(byte[] sessionId,
                                     int sessionMessageType,
                                     byte[] request) {
            mProxyCallbacks.onSessionMessage(sessionId, sessionMessageType, request);
        }

        @Override
        public void onSessionError(byte[] sessionId, String message) {
            mProxyCallbacks.onSessionError(sessionId, message);
        }

        @Override
        public void onSessionBatchedKeyChanged(byte[] sessionId,
                                               SessionKeyInfo[] keyInfos) {
            mProxyCallbacks.onSessionBatchedKeyChanged(sessionId, keyInfos);
        }

        @Override
        public void onRejectPromise(int promiseId, String message) {
            mProxyCallbacks.onRejectPromise(promiseId, message);
        }
    } // CallbacksForwarder

    /* package-private */ static void assertTrue(boolean condition) {
        if (DEBUG && !condition) {
            throw new AssertionError("Expected condition to be true");
        }
    }

    public RemoteMediaDrmBridge(IMediaDrmBridge remoteBridge) {
        assertTrue(remoteBridge != null);
        mRemote = remoteBridge;
    }

    @Override
    public synchronized void setCallbacks(Callbacks callbacks) {
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
    public synchronized void createSession(int createSessionToken,
                                           int promiseId,
                                           String initDataType,
                                           byte[] initData) {
        if (DEBUG) Log.d(LOGTAG, "createSession()");

        try {
            mRemote.createSession(createSessionToken, promiseId, initDataType, initData);
        } catch (Exception e) {
            Log.e(LOGTAG, "Got exception while creating remote session.", e);
            mCallbacksFwd.onRejectPromise(promiseId, "Failed to create session.");
        }
    }

    @Override
    public synchronized void updateSession(int promiseId, String sessionId, byte[] response) {
        if (DEBUG) Log.d(LOGTAG, "updateSession()");

        try {
            mRemote.updateSession(promiseId, sessionId, response);
        } catch (Exception e) {
            Log.e(LOGTAG, "Got exception while updating remote session.", e);
            mCallbacksFwd.onRejectPromise(promiseId, "Failed to update session.");
        }
    }

    @Override
    public synchronized void closeSession(int promiseId, String sessionId) {
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
        mRemote = null;
        mCallbacksFwd = null;
    }

    @Override
    public synchronized MediaCrypto getMediaCrypto() {
        if (DEBUG) Log.d(LOGTAG, "getMediaCrypto(), should not enter here!");
        assertTrue(false);
        return null;
    }
}