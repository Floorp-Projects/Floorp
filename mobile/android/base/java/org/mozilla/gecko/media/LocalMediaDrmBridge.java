/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;
import org.mozilla.gecko.AppConstants;

import android.media.MediaCrypto;
import android.util.Log;

final class LocalMediaDrmBridge implements GeckoMediaDrm {
    private static final String LOGTAG = "GeckoLocalMediaDrmBridge";
    private static final boolean DEBUG = false;
    private GeckoMediaDrm mBridge = null;
    private CallbacksForwarder mCallbacksFwd;

    // Forward the callback calls from GeckoMediaDrmBridgeV{21,23}
    // to the callback MediaDrmProxy.Callbacks.
    private class CallbacksForwarder implements GeckoMediaDrm.Callbacks {
        private final GeckoMediaDrm.Callbacks mProxyCallbacks;

        CallbacksForwarder(GeckoMediaDrm.Callbacks callbacks) {
            assertTrue(callbacks != null);
            mProxyCallbacks = callbacks;
        }

        @Override
        public void onSessionCreated(int createSessionToken,
                                     int promiseId,
                                     byte[] sessionId,
                                     byte[] request) {
            assertTrue(mProxyCallbacks != null);
            mProxyCallbacks.onSessionCreated(createSessionToken,
                                             promiseId,
                                             sessionId,
                                             request);
        }

        @Override
        public void onSessionUpdated(int promiseId, byte[] sessionId) {
            assertTrue(mProxyCallbacks != null);
            mProxyCallbacks.onSessionUpdated(promiseId, sessionId);
        }

        @Override
        public void onSessionClosed(int promiseId, byte[] sessionId) {
            assertTrue(mProxyCallbacks != null);
            mProxyCallbacks.onSessionClosed(promiseId, sessionId);
        }

        @Override
        public void onSessionMessage(byte[] sessionId,
                                     int sessionMessageType,
                                     byte[] request) {
            assertTrue(mProxyCallbacks != null);
            mProxyCallbacks.onSessionMessage(sessionId, sessionMessageType, request);
        }

        @Override
        public void onSessionError(byte[] sessionId,
                                   String message) {
            assertTrue(mProxyCallbacks != null);
            mProxyCallbacks.onSessionError(sessionId, message);
        }

        @Override
        public void onSessionBatchedKeyChanged(byte[] sessionId,
                                               SessionKeyInfo[] keyInfos) {
            assertTrue(mProxyCallbacks != null);
            mProxyCallbacks.onSessionBatchedKeyChanged(sessionId, keyInfos);
        }

        @Override
        public void onRejectPromise(int promiseId, String message) {
            if (DEBUG) Log.d(LOGTAG, message);
            assertTrue(mProxyCallbacks != null);
            mProxyCallbacks.onRejectPromise(promiseId, message);
        }
    } // CallbacksForwarder

    private static void assertTrue(boolean condition) {
        if (DEBUG && !condition) {
          throw new AssertionError("Expected condition to be true");
        }
      }

    LocalMediaDrmBridge(String keySystem) throws Exception {
        if (AppConstants.Versions.preLollipop) {
            mBridge = null;
        } else if (AppConstants.Versions.feature21Plus &&
                   AppConstants.Versions.preMarshmallow) {
            mBridge = new GeckoMediaDrmBridgeV21(keySystem);
        } else {
            mBridge = new GeckoMediaDrmBridgeV23(keySystem);
        }
    }

    @Override
    public synchronized void setCallbacks(Callbacks callbacks) {
        if (DEBUG) Log.d(LOGTAG, "setCallbacks()");
        mCallbacksFwd = new CallbacksForwarder(callbacks);
        assertTrue(mBridge != null);
        mBridge.setCallbacks(mCallbacksFwd);
    }

    @Override
    public synchronized void createSession(int createSessionToken,
                                           int promiseId,
                                           String initDataType,
                                           byte[] initData) {
        if (DEBUG) Log.d(LOGTAG, "createSession()");
        assertTrue(mCallbacksFwd != null);
        try {
            mBridge.createSession(createSessionToken, promiseId, initDataType, initData);
        } catch (Exception e) {
            Log.e(LOGTAG, "Failed to createSession.", e);
            mCallbacksFwd.onRejectPromise(promiseId, "Failed to createSession.");
        }
    }

    @Override
    public synchronized void updateSession(int promiseId, String sessionId, byte[] response) {
        if (DEBUG) Log.d(LOGTAG, "updateSession()");
        assertTrue(mCallbacksFwd != null);
        try {
            mBridge.updateSession(promiseId, sessionId, response);
        } catch (Exception e) {
            Log.e(LOGTAG, "Failed to updateSession.", e);
            mCallbacksFwd.onRejectPromise(promiseId, "Failed to updateSession.");
        }
    }

    @Override
    public synchronized void closeSession(int promiseId, String sessionId) {
        if (DEBUG) Log.d(LOGTAG, "closeSession()");
        assertTrue(mCallbacksFwd != null);
        try {
            mBridge.closeSession(promiseId, sessionId);
        } catch (Exception e) {
            Log.e(LOGTAG, "Failed to closeSession.", e);
            mCallbacksFwd.onRejectPromise(promiseId, "Failed to closeSession.");
        }
    }

    @Override
    public synchronized void release() {
        if (DEBUG) Log.d(LOGTAG, "release()");
        try {
            mBridge.release();
            mBridge = null;
            mCallbacksFwd = null;
        } catch (Exception e) {
            Log.e(LOGTAG, "Failed to release", e);
        }
    }

    @Override
    public synchronized MediaCrypto getMediaCrypto() {
        if (DEBUG) Log.d(LOGTAG, "getMediaCrypto()");
        return mBridge != null ? mBridge.getMediaCrypto() : null;
    }
}
