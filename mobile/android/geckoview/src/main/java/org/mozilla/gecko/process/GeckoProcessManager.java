/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.process;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.mozglue.GeckoLoader;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.DeadObjectException;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.support.v4.util.SimpleArrayMap;
import android.view.Surface;
import android.util.Log;

import java.io.IOException;
import java.util.Collections;
import java.util.concurrent.TimeUnit;
import java.util.Map;

public final class GeckoProcessManager {
    private static final String LOGTAG = "GeckoProcessManager";
    private static final GeckoProcessManager INSTANCE = new GeckoProcessManager();

    public static GeckoProcessManager getInstance() {
        return INSTANCE;
    }

    private static final class ChildConnection implements ServiceConnection, IBinder.DeathRecipient {
        public final String mType;
        private boolean mWait = false;
        public IChildProcess mChild = null;
        public int mPid = 0;
        public ChildConnection(String type) {
            mType = type;
        }

        void prepareToWait() {
            mWait = true;
        }

        void waitForChild() {
            ThreadUtils.assertNotOnUiThread();
            synchronized(this) {
                if (mWait) {
                    try {
                        this.wait(5000); // 5 seconds
                    } catch (final InterruptedException e) {
                        Log.e(LOGTAG, "Interrupted while waiting for child service", e);
                    }
                }
            }
        }

        void clearWait() {
            mWait = false;
        }

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            try {
                service.linkToDeath(this, 0);
            } catch (final RemoteException e) {
                Log.e(LOGTAG, "Failed to link ChildConnection to death of service.", e);
            }
            mChild = IChildProcess.Stub.asInterface(service);
            try {
                mPid = mChild.getPid();
            } catch (final RemoteException e) {
                Log.e(LOGTAG, "Failed to get child " + mType + " process PID. Process may have died.", e);
            }
            synchronized(this) {
                if (mWait) {
                    mWait = false;
                    this.notifyAll();
                }
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            synchronized(INSTANCE.mConnections) {
                INSTANCE.mConnections.remove(mType);
            }
            synchronized(this) {
                if (mWait) {
                    mWait = false;
                    this.notifyAll();
                }
            }
        }

        @Override
        public void binderDied() {
            Log.e(LOGTAG,"Binder died. Attempt to unbind service: " + mType + " " + mPid);
            try {
                GeckoAppShell.getApplicationContext().unbindService(this);
            } catch (final java.lang.IllegalArgumentException e) {
                Log.e(LOGTAG,"Looks like connection was already unbound", e);
            }
        }
    }

    SimpleArrayMap<String, ChildConnection> mConnections;

    private GeckoProcessManager() {
        mConnections = new SimpleArrayMap<String, ChildConnection>();
    }

    public int start(String type, String[] args, int crashFd, int ipcFd) {
        ChildConnection connection = null;
        synchronized(mConnections) {
            connection = mConnections.get(type);
        }
        if (connection != null) {
            Log.w(LOGTAG, "Attempting to start a child process service that is already running. Attempting to kill existing process first");
            connection.prepareToWait();
            try {
                connection.mChild.stop();
                connection.waitForChild();
            } catch (final RemoteException e) {
                connection.clearWait();
            }
        }

        try {
            connection = new ChildConnection(type);
            Intent intent = new Intent();
            intent.setClassName(GeckoAppShell.getApplicationContext(),
                                "org.mozilla.gecko.process.GeckoServiceChildProcess$" + type);
            GeckoLoader.addEnvironmentToIntent(intent);
            connection.prepareToWait();
            GeckoAppShell.getApplicationContext().bindService(intent, connection, Context.BIND_AUTO_CREATE);
            connection.waitForChild();
            if (connection.mChild == null) {
                // FAILED TO CONNECT.
                Log.e(LOGTAG, "Failed to connect to child process of '" + type + "'");
                GeckoAppShell.getApplicationContext().unbindService(connection);
                return 0;
            }
            ParcelFileDescriptor crashPfd = null;
            if (crashFd >= 0) {
                crashPfd = ParcelFileDescriptor.fromFd(crashFd);
            }
            ParcelFileDescriptor ipcPfd = ParcelFileDescriptor.fromFd(ipcFd);
            connection.mChild.start(args, crashPfd, ipcPfd);
            if (crashPfd != null) {
                crashPfd.close();
            }
            ipcPfd.close();
            synchronized(mConnections) {
                mConnections.put(type, connection);
            }
            return connection.mPid;
        } catch (final RemoteException e) {
            Log.e(LOGTAG, "Unable to create child process for: '" + type + "'. Remote Exception:", e);
        } catch (final IOException e) {
            Log.e(LOGTAG, "Unable to create child process for: '" + type + "'. Error creating ParcelFileDescriptor needed to create intent:", e);
        }

        return 0;
    }

} // GeckoProcessManager
