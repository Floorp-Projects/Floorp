/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.process;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.IGeckoEditableParent;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.os.RemoteException;
import android.os.SystemClock;
import android.support.v4.util.SimpleArrayMap;
import android.util.Log;

import java.io.IOException;

public final class GeckoProcessManager extends IProcessManager.Stub {
    private static final String LOGTAG = "GeckoProcessManager";
    private static final GeckoProcessManager INSTANCE = new GeckoProcessManager();

    public static GeckoProcessManager getInstance() {
        return INSTANCE;
    }

    @WrapForJNI(stubName = "GetEditableParent")
    private static native IGeckoEditableParent nativeGetEditableParent(long contentId,
                                                                       long tabId);

    @Override // IProcessManager
    public IGeckoEditableParent getEditableParent(final long contentId, final long tabId) {
        return nativeGetEditableParent(contentId, tabId);
    }

    private static final class ChildConnection implements ServiceConnection,
                                                          IBinder.DeathRecipient {
        private static final int WAIT_TIMEOUT = 5000; // 5 seconds

        private final String mType;
        private boolean mWaiting;
        private IChildProcess mChild;
        private int mPid;

        public ChildConnection(String type) {
            mType = type;
        }

        public synchronized int getPid() {
            if ((mPid == 0) && (mChild != null)) {
                try {
                    mPid = mChild.getPid();
                } catch (final RemoteException e) {
                    Log.e(LOGTAG, "Cannot get pid for " + mType, e);
                }
            }
            return mPid;
        }

        public synchronized IChildProcess bind() {
            if (mChild != null) {
                return mChild;
            }

            final Context context = GeckoAppShell.getApplicationContext();
            final Intent intent = new Intent();
            intent.setClassName(context,
                                GeckoServiceChildProcess.class.getName() + '$' + mType);

            if (context.bindService(intent, this, Context.BIND_AUTO_CREATE)) {
                waitForChildLocked();
                if (mChild != null) {
                    return mChild;
                }
            }

            Log.e(LOGTAG, "Cannot connect to process " + mType);
            context.unbindService(this);
            return null;
        }

        public synchronized void unbind() {
            if (mChild != null) {
                final Context context = GeckoAppShell.getApplicationContext();
                context.unbindService(this);
            }

            final int pid = getPid();
            if (pid != 0) {
                Process.killProcess(pid);
                waitForChildLocked();
            }
        }

        private void waitForChildLocked() {
            ThreadUtils.assertNotOnUiThread();
            mWaiting = true;

            final long startTime = SystemClock.uptimeMillis();
            while (mWaiting && SystemClock.uptimeMillis() - startTime < WAIT_TIMEOUT) {
                try {
                    wait(WAIT_TIMEOUT);
                } catch (final InterruptedException e) {
                }
            }
            mWaiting = false;
        }

        @Override
        public synchronized void onServiceConnected(ComponentName name, IBinder service) {
            try {
                service.linkToDeath(this, 0);
            } catch (final RemoteException e) {
                Log.e(LOGTAG, "Cannot link to death for " + mType, e);
            }

            mChild = IChildProcess.Stub.asInterface(service);
            mWaiting = false;
            notifyAll();
        }

        @Override
        public synchronized void onServiceDisconnected(ComponentName name) {
            mChild = null;
            mPid = 0;
            mWaiting = false;
            notifyAll();
        }

        @Override
        public synchronized void binderDied() {
            Log.i(LOGTAG, "Binder died for " + mType);
            if (mChild != null) {
                mChild = null;
                mPid = 0;
                GeckoAppShell.getApplicationContext().unbindService(this);
            }
        }
    }

    private final SimpleArrayMap<String, ChildConnection> mConnections;

    private GeckoProcessManager() {
        mConnections = new SimpleArrayMap<String, ChildConnection>();
    }

    private ChildConnection getConnection(final String type) {
        synchronized (mConnections) {
            ChildConnection connection = mConnections.get(type);
            if (connection == null) {
                connection = new ChildConnection(type);
                mConnections.put(type, connection);
            }
            return connection;
        }
    }

    public void preload(final String... types) {
        for (final String type : types) {
            final ChildConnection connection = getConnection(type);
            if (connection.bind() != null) {
                connection.getPid();
            }
        }
    }

    public void crashChild() {
        try {
            mConnections.get("tab").bind().crash();
        } catch (RemoteException e) {
        }
    }

    @WrapForJNI
    private static int start(final String type, final String[] args,
                             final int prefsFd, final int prefMapFd,
                             final int ipcFd,
                             final int crashFd, final int crashAnnotationFd) {
        return INSTANCE.start(type, args, prefsFd, prefMapFd, ipcFd, crashFd, crashAnnotationFd, /* retry */ false);
    }

    private int filterFlagsForChild(int flags) {
        return flags & (GeckoThread.FLAG_ENABLE_JAVA_CRASHREPORTER |
                GeckoThread.FLAG_ENABLE_NATIVE_CRASHREPORTER);
    }

    private int start(final String type, final String[] args,
                      final int prefsFd, final int prefMapFd,
                      final int ipcFd, final int crashFd,
                      final int crashAnnotationFd, final boolean retry) {
        final ChildConnection connection = getConnection(type);
        final IChildProcess child = connection.bind();
        if (child == null) {
            return 0;
        }

        final Bundle extras = GeckoThread.getActiveExtras();
        final ParcelFileDescriptor prefsPfd;
        final ParcelFileDescriptor prefMapPfd;
        final ParcelFileDescriptor ipcPfd;
        final ParcelFileDescriptor crashPfd;
        final ParcelFileDescriptor crashAnnotationPfd;
        try {
            prefsPfd = ParcelFileDescriptor.fromFd(prefsFd);
            prefMapPfd = ParcelFileDescriptor.fromFd(prefMapFd);
            ipcPfd = ParcelFileDescriptor.fromFd(ipcFd);
            crashPfd = (crashFd >= 0) ? ParcelFileDescriptor.fromFd(crashFd) : null;
            crashAnnotationPfd = (crashAnnotationFd >= 0) ? ParcelFileDescriptor.fromFd(crashAnnotationFd) : null;
        } catch (final IOException e) {
            Log.e(LOGTAG, "Cannot create fd for " + type, e);
            return 0;
        }

        final int flags = filterFlagsForChild(GeckoThread.getActiveFlags());

        boolean started = false;
        try {
            started = child.start(this, args, extras, flags, prefsPfd, prefMapPfd,
                                  ipcPfd, crashPfd, crashAnnotationPfd);
        } catch (final RemoteException e) {
        }

        if (!started) {
            if (retry) {
                Log.e(LOGTAG, "Cannot restart child " + type);
                return 0;
            }
            Log.w(LOGTAG, "Attempting to kill running child " + type);
            connection.unbind();
            return start(type, args, prefsFd, prefMapFd, ipcFd, crashFd, crashAnnotationFd, /* retry */ true);
        }

        try {
            if (crashAnnotationPfd != null) {
                crashAnnotationPfd.close();
            }
            if (crashPfd != null) {
                crashPfd.close();
            }
            ipcPfd.close();
        } catch (final IOException e) {
        }

        return connection.getPid();
    }

} // GeckoProcessManager
