/*
 * Copyright (C) 2012 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

package com.googlecode.eyesfree.braille.selfbraille;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.Signature;
import android.os.Binder;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.RemoteException;
import android.util.Log;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

/**
 * Client-side interface to the self brailling interface.
 *
 * Threading: Instances of this object should be created and shut down
 * in a thread with a {@link Looper} associated with it.  Other methods may
 * be called on any thread.
 */
public class SelfBrailleClient {
    private static final String LOG_TAG =
            SelfBrailleClient.class.getSimpleName();
    private static final String ACTION_SELF_BRAILLE_SERVICE =
            "com.googlecode.eyesfree.braille.service.ACTION_SELF_BRAILLE_SERVICE";
    private static final String BRAILLE_BACK_PACKAGE =
            "com.googlecode.eyesfree.brailleback";
    private static final Intent mServiceIntent =
            new Intent(ACTION_SELF_BRAILLE_SERVICE)
            .setPackage(BRAILLE_BACK_PACKAGE);
    /**
     * SHA-1 hash value of the Eyes-Free release key certificate, used to sign
     * BrailleBack.  It was generated from the keystore with:
     * $ keytool -exportcert -keystore <keystorefile> -alias android.keystore \
     *   > cert
     * $ keytool -printcert -file cert
     */
    // The typecasts are to silence a compiler warning about loss of precision
    private static final byte[] EYES_FREE_CERT_SHA1 = new byte[] {
        (byte) 0x9B, (byte) 0x42, (byte) 0x4C, (byte) 0x2D,
        (byte) 0x27, (byte) 0xAD, (byte) 0x51, (byte) 0xA4,
        (byte) 0x2A, (byte) 0x33, (byte) 0x7E, (byte) 0x0B,
        (byte) 0xB6, (byte) 0x99, (byte) 0x1C, (byte) 0x76,
        (byte) 0xEC, (byte) 0xA4, (byte) 0x44, (byte) 0x61
    };
    /**
     * Delay before the first rebind attempt on bind error or service
     * disconnect.
     */
    private static final int REBIND_DELAY_MILLIS = 500;
    private static final int MAX_REBIND_ATTEMPTS = 5;

    private final Binder mIdentity = new Binder();
    private final Context mContext;
    private final boolean mAllowDebugService;
    private final SelfBrailleHandler mHandler = new SelfBrailleHandler();
    private boolean mShutdown = false;

    /**
     * Written in handler thread, read in any thread calling methods on the
     * object.
     */
    private volatile Connection mConnection;
    /** Protected by synchronizing on mHandler. */
    private int mNumFailedBinds = 0;

    /**
     * Constructs an instance of this class.  {@code context} is used to bind
     * to the self braille service.  The current thread must have a Looper
     * associated with it.  If {@code allowDebugService} is true, this instance
     * will connect to a BrailleBack service without requiring it to be signed
     * by the release key used to sign BrailleBack.
     */
    public SelfBrailleClient(Context context, boolean allowDebugService) {
        mContext = context;
        mAllowDebugService = allowDebugService;
        doBindService();
    }

    /**
     * Shuts this instance down, deallocating any global resources it is using.
     * This method must be called on the same thread that created this object.
     */
    public void shutdown() {
        mShutdown = true;
        doUnbindService();
    }

    public void write(WriteData writeData) {
        writeData.validate();
        ISelfBrailleService localService = getSelfBrailleService();
        if (localService != null) {
            try {
                localService.write(mIdentity, writeData);
            } catch (RemoteException ex) {
                Log.e(LOG_TAG, "Self braille write failed", ex);
            }
        }
    }

    private void doBindService() {
        Connection localConnection = new Connection();
        if (!mContext.bindService(mServiceIntent, localConnection,
                Context.BIND_AUTO_CREATE)) {
            Log.e(LOG_TAG, "Failed to bind to service");
            mHandler.scheduleRebind();
            return;
        }
        mConnection = localConnection;
        Log.i(LOG_TAG, "Bound to self braille service");
    }

    private void doUnbindService() {
        if (mConnection != null) {
            ISelfBrailleService localService = getSelfBrailleService();
            if (localService != null) {
                try {
                    localService.disconnect(mIdentity);
                } catch (RemoteException ex) {
                    // Nothing to do.
                }
            }
            mContext.unbindService(mConnection);
            mConnection = null;
        }
    }

    private ISelfBrailleService getSelfBrailleService() {
        Connection localConnection = mConnection;
        if (localConnection != null) {
            return localConnection.mService;
        }
        return null;
    }

    private boolean verifyPackage() {
        PackageManager pm = mContext.getPackageManager();
        PackageInfo pi;
        try {
            pi = pm.getPackageInfo(BRAILLE_BACK_PACKAGE,
                    PackageManager.GET_SIGNATURES);
        } catch (PackageManager.NameNotFoundException ex) {
            Log.w(LOG_TAG, "Can't verify package " + BRAILLE_BACK_PACKAGE,
                    ex);
            return false;
        }
        MessageDigest digest;
        try {
            digest = MessageDigest.getInstance("SHA-1");
        } catch (NoSuchAlgorithmException ex) {
            Log.e(LOG_TAG, "SHA-1 not supported", ex);
            return false;
        }
        // Check if any of the certificates match our hash.
        for (Signature signature : pi.signatures) {
            digest.update(signature.toByteArray());
            if (MessageDigest.isEqual(EYES_FREE_CERT_SHA1, digest.digest())) {
                return true;
            }
            digest.reset();
        }
        if (mAllowDebugService) {
            Log.w(LOG_TAG, String.format(
                "*** %s connected to BrailleBack with invalid (debug?) "
                + "signature ***",
                mContext.getPackageName()));
            return true;
        }
        return false;
    }
    private class Connection implements ServiceConnection {
        // Read in application threads, written in main thread.
        private volatile ISelfBrailleService mService;

        @Override
        public void onServiceConnected(ComponentName className,
                IBinder binder) {
            if (!verifyPackage()) {
                Log.w(LOG_TAG, String.format("Service certificate mismatch "
                                + "for %s, dropping connection",
                                BRAILLE_BACK_PACKAGE));
                mHandler.unbindService();
                return;
            }
            Log.i(LOG_TAG, "Connected to self braille service");
            mService = ISelfBrailleService.Stub.asInterface(binder);
            synchronized (mHandler) {
                mNumFailedBinds = 0;
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName className) {
            Log.e(LOG_TAG, "Disconnected from self braille service");
            mService = null;
            // Retry by rebinding.
            mHandler.scheduleRebind();
        }
    }

    private class SelfBrailleHandler extends Handler {
        private static final int MSG_REBIND_SERVICE = 1;
        private static final int MSG_UNBIND_SERVICE = 2;

        public void scheduleRebind() {
            synchronized (this) {
                if (mNumFailedBinds < MAX_REBIND_ATTEMPTS) {
                    int delay = REBIND_DELAY_MILLIS << mNumFailedBinds;
                    sendEmptyMessageDelayed(MSG_REBIND_SERVICE, delay);
                    ++mNumFailedBinds;
                }
            }
        }

        public void unbindService() {
            sendEmptyMessage(MSG_UNBIND_SERVICE);
        }

        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case MSG_REBIND_SERVICE:
                    handleRebindService();
                    break;
                case MSG_UNBIND_SERVICE:
                    handleUnbindService();
                    break;
            }
        }

        private void handleRebindService() {
            if (mShutdown) {
                return;
            }
            if (mConnection != null) {
                doUnbindService();
            }
            doBindService();
        }

        private void handleUnbindService() {
            doUnbindService();
        }
    }
}
