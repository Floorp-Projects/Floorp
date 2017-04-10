/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;
import org.mozilla.gecko.AppConstants;

import java.util.ArrayList;

import android.media.MediaCrypto;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;

final class RemoteMediaDrmBridgeStub extends IMediaDrmBridge.Stub implements IBinder.DeathRecipient {
    private static final String LOGTAG = "GeckoRemoteMediaDrmBridgeStub";
    private static final boolean DEBUG = false;
    private volatile IMediaDrmBridgeCallbacks mCallbacks = null;

    // Underlying bridge implmenetaion, i.e. GeckoMediaDrmBrdigeV21.
    private GeckoMediaDrm mBridge = null;

    // mStubId is initialized during stub construction. It should be a unique
    // string which is generated in MediaDrmProxy in Fennec App process and is
    // used for Codec to obtain corresponding MediaCrypto as input to achieve
    // decryption.
    // The generated stubId will be delivered to Codec via a code path starting
    // from MediaDrmProxy -> MediaDrmCDMProxy -> RemoteDataDecoder => IPC => Codec.
    private String mStubId = "";

    public static ArrayList<RemoteMediaDrmBridgeStub> mBridgeStubs =
        new ArrayList<RemoteMediaDrmBridgeStub>();

    private String getId() {
        return mStubId;
    }

    private MediaCrypto getMediaCryptoFromBridge() {
        return mBridge != null ? mBridge.getMediaCrypto() : null;
    }

    public static synchronized MediaCrypto getMediaCrypto(String stubId) {
        if (DEBUG) Log.d(LOGTAG, "getMediaCrypto()");

        for (int i = 0; i < mBridgeStubs.size(); i++) {
            if (mBridgeStubs.get(i) != null &&
                mBridgeStubs.get(i).getId().equals(stubId)) {
                return mBridgeStubs.get(i).getMediaCryptoFromBridge();
            }
        }
        return null;
    }

    // Callback to RemoteMediaDrmBridge.
    private final class Callbacks implements GeckoMediaDrm.Callbacks {
        private IMediaDrmBridgeCallbacks mRemoteCallbacks;

        public Callbacks(IMediaDrmBridgeCallbacks remote) {
            mRemoteCallbacks = remote;
        }

        @Override
        public void onSessionCreated(int createSessionToken,
                                     int promiseId,
                                     byte[] sessionId,
                                     byte[] request) {
            if (DEBUG) Log.d(LOGTAG, "onSessionCreated()");
            try {
                mRemoteCallbacks.onSessionCreated(createSessionToken,
                                                  promiseId,
                                                  sessionId,
                                                  request);
            } catch (RemoteException e) {
                Log.e(LOGTAG, "Exception ! Dead recipient !!", e);
            }
        }

        @Override
        public void onSessionUpdated(int promiseId, byte[] sessionId) {
            if (DEBUG) Log.d(LOGTAG, "onSessionUpdated()");
            try {
                mRemoteCallbacks.onSessionUpdated(promiseId, sessionId);
            } catch (RemoteException e) {
                Log.e(LOGTAG, "Exception ! Dead recipient !!", e);
            }
        }

        @Override
        public void onSessionClosed(int promiseId, byte[] sessionId) {
            if (DEBUG) Log.d(LOGTAG, "onSessionClosed()");
            try {
                mRemoteCallbacks.onSessionClosed(promiseId, sessionId);
            } catch (RemoteException e) {
                Log.e(LOGTAG, "Exception ! Dead recipient !!", e);
            }
        }

        @Override
        public void onSessionMessage(byte[] sessionId,
                                     int sessionMessageType,
                                     byte[] request) {
            if (DEBUG) Log.d(LOGTAG, "onSessionMessage()");
            try {
                mRemoteCallbacks.onSessionMessage(sessionId, sessionMessageType, request);
            } catch (RemoteException e) {
                Log.e(LOGTAG, "Exception ! Dead recipient !!", e);
            }
        }

        @Override
        public void onSessionError(byte[] sessionId, String message) {
            if (DEBUG) Log.d(LOGTAG, "onSessionError()");
            try {
                mRemoteCallbacks.onSessionError(sessionId, message);
            } catch (RemoteException e) {
                Log.e(LOGTAG, "Exception ! Dead recipient !!", e);
            }
        }

        @Override
        public void onSessionBatchedKeyChanged(byte[] sessionId,
                                               SessionKeyInfo[] keyInfos) {
            if (DEBUG) Log.d(LOGTAG, "onSessionBatchedKeyChanged()");
            try {
                mRemoteCallbacks.onSessionBatchedKeyChanged(sessionId, keyInfos);
            } catch (RemoteException e) {
                Log.e(LOGTAG, "Exception ! Dead recipient !!", e);
            }
        }

        @Override
        public void onRejectPromise(int promiseId, String message) {
            if (DEBUG) Log.d(LOGTAG, "onRejectPromise()");
            try {
                mRemoteCallbacks.onRejectPromise(promiseId, message);
            } catch (RemoteException e) {
                Log.e(LOGTAG, "Exception ! Dead recipient !!", e);
            }
        }
    }

    /* package-private */ void assertTrue(boolean condition) {
        if (DEBUG && !condition) {
            throw new AssertionError("Expected condition to be true");
        }
    }

    RemoteMediaDrmBridgeStub(String keySystem, String stubId) throws RemoteException {
        if (AppConstants.Versions.preLollipop) {
            Log.e(LOGTAG, "Pre-Lollipop should never enter here!!");
            throw new RemoteException("Error, unsupported version!");
        }
        try {
            if (AppConstants.Versions.preMarshmallow) {
                mBridge = new GeckoMediaDrmBridgeV21(keySystem);
            } else {
                mBridge = new GeckoMediaDrmBridgeV23(keySystem);
            }
            mStubId = stubId;
            mBridgeStubs.add(this);
        } catch (Exception e) {
            throw new RemoteException("RemoteMediaDrmBridgeStub cannot create bridge implementation.");
        }
    }

    @Override
    public synchronized void setCallbacks(IMediaDrmBridgeCallbacks callbacks) throws RemoteException {
        if (DEBUG) Log.d(LOGTAG, "setCallbacks()");
        assertTrue(mBridge != null);
        assertTrue(callbacks != null);
        mCallbacks = callbacks;
        callbacks.asBinder().linkToDeath(this, 0);
        mBridge.setCallbacks(new Callbacks(mCallbacks));
    }

    @Override
    public synchronized void createSession(int createSessionToken,
                                           int promiseId,
                                           String initDataType,
                                           byte[] initData) throws RemoteException {
        if (DEBUG) Log.d(LOGTAG, "createSession()");
        try {
            assertTrue(mCallbacks != null);
            assertTrue(mBridge != null);
            mBridge.createSession(createSessionToken,
                                  promiseId,
                                  initDataType,
                                  initData);
        } catch (Exception e) {
            Log.e(LOGTAG, "Failed to createSession.", e);
            mCallbacks.onRejectPromise(promiseId, "Failed to createSession.");
        }
    }

    @Override
    public synchronized void updateSession(int promiseId,
                                           String sessionId,
                                           byte[] response) throws RemoteException {
        if (DEBUG) Log.d(LOGTAG, "updateSession()");
        try {
            assertTrue(mCallbacks != null);
            assertTrue(mBridge != null);
            mBridge.updateSession(promiseId, sessionId, response);
        } catch (Exception e) {
            Log.e(LOGTAG, "Failed to updateSession.", e);
            mCallbacks.onRejectPromise(promiseId, "Failed to updateSession.");
        }
    }

    @Override
    public synchronized void closeSession(int promiseId, String sessionId) throws RemoteException {
        if (DEBUG) Log.d(LOGTAG, "closeSession()");
        try {
            assertTrue(mCallbacks != null);
            assertTrue(mBridge != null);
            mBridge.closeSession(promiseId, sessionId);
        } catch (Exception e) {
            Log.e(LOGTAG, "Failed to closeSession.", e);
            mCallbacks.onRejectPromise(promiseId, "Failed to closeSession.");
        }
    }

    // IBinder.DeathRecipient
    @Override
    public synchronized void binderDied() {
        Log.e(LOGTAG, "Binder died !!");
        try {
            release();
        } catch (Exception e) {
            Log.e(LOGTAG, "Exception ! Dead recipient !!", e);
        }
    }

    @Override
    public synchronized void release() {
        if (DEBUG) Log.d(LOGTAG, "release()");
        mBridgeStubs.remove(this);
        if (mBridge != null) {
            mBridge.release();
            mBridge = null;
        }
        mCallbacks.asBinder().unlinkToDeath(this, 0);
        mCallbacks = null;
        mStubId = "";
    }
}
