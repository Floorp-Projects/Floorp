/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.process;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.IGeckoEditableChild;
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

public final class GeckoProcessManager extends IProcessManager.Stub {
    private static final String LOGTAG = "GeckoProcessManager";
    private static final GeckoProcessManager INSTANCE = new GeckoProcessManager();

    public static GeckoProcessManager getInstance() {
        return INSTANCE;
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void setEditableChildParent(final IGeckoEditableChild child,
                                               final IGeckoEditableParent parent) {
        try {
            child.transferParent(parent);
        } catch (final RemoteException e) {
            Log.e(LOGTAG, "Cannot set parent", e);
        }
    }

    @WrapForJNI(stubName = "GetEditableParent", dispatchTo = "gecko")
    private static native void nativeGetEditableParent(IGeckoEditableChild child,
                                                       long contentId, long tabId);

    @Override // IProcessManager
    public void getEditableParent(final IGeckoEditableChild child,
                                  final long contentId, final long tabId) {
        nativeGetEditableParent(child, contentId, tabId);
    }

    private static final class ChildConnection implements ServiceConnection,
                                                          IBinder.DeathRecipient {
        private static final int WAIT_TIMEOUT = 5000; // 5 seconds

        private final String mType;
        private boolean mWaiting;
        private IChildProcess mChild;
        private int mPid;

        public ChildConnection(final String type) {
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

            if (context.bindService(intent, this, Context.BIND_AUTO_CREATE | Context.BIND_IMPORTANT)) {
                waitForChildLocked();
                if (mChild != null) {
                    return mChild;
                }
            }

            Log.e(LOGTAG, "Cannot connect to process " + mType);
            unbind();
            return null;
        }

        public synchronized void unbind() {
            // This could end up using IPC, so do it before we unbind.
            final int pid = getPid();

            if (mChild != null) {
                final Context context = GeckoAppShell.getApplicationContext();
                try {
                    context.unbindService(this);
                } catch (IllegalArgumentException e) {
                    mChild = null;
                    mPid = 0;
                    return;
                }
            }

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
        public synchronized void onServiceConnected(final ComponentName name,
                                                    final IBinder service) {
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
        public synchronized void onServiceDisconnected(final ComponentName name) {
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

                try {
                    GeckoAppShell.getApplicationContext().unbindService(this);
                } catch (IllegalArgumentException e) {
                }
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
            IChildProcess childProcess = mConnections.get("tab").bind();
            if (childProcess != null) {
                childProcess.crash();
            }
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

    private int filterFlagsForChild(final int flags) {
        return flags & GeckoThread.FLAG_ENABLE_NATIVE_CRASHREPORTER;
    }

    private int start(final String type, final String[] args,
                      final int prefsFd, final int prefMapFd,
                      final int ipcFd, final int crashFd,
                      final int crashAnnotationFd, final boolean retry) {
        return start(type, args, prefsFd, prefMapFd, ipcFd, crashFd, crashAnnotationFd, retry, /* prevException */ null);
    }

    private int start(final String type, final String[] args,
                      final int prefsFd, final int prefMapFd,
                      final int ipcFd, final int crashFd,
                      final int crashAnnotationFd, final boolean retry,
                      final RemoteException prevException) {
        final ChildConnection connection = getConnection(type);
        final IChildProcess child = connection.bind();
        if (child == null) {
            final StringBuilder builder = new StringBuilder("Cannot bind child process.");
            if (prevException != null) {
                builder.append(" Previous exception: " + prevException.toString());
            }
            builder.append(" Type: " + type);
            throw new RuntimeException(builder.toString());
        }

        final Bundle extras = GeckoThread.getActiveExtras();
        final ParcelFileDescriptor prefsPfd =
                (prefsFd >= 0) ? ParcelFileDescriptor.adoptFd(prefsFd) : null;
        final ParcelFileDescriptor prefMapPfd =
                (prefMapFd >= 0) ? ParcelFileDescriptor.adoptFd(prefMapFd) : null;
        final ParcelFileDescriptor ipcPfd = ParcelFileDescriptor.adoptFd(ipcFd);
        final ParcelFileDescriptor crashPfd =
                (crashFd >= 0) ? ParcelFileDescriptor.adoptFd(crashFd) : null;
        final ParcelFileDescriptor crashAnnotationPfd =
                (crashAnnotationFd >= 0) ? ParcelFileDescriptor.adoptFd(crashAnnotationFd) : null;

        final int flags = filterFlagsForChild(GeckoThread.getActiveFlags());

        boolean started = false;
        RemoteException exception = null;
        final String crashHandler = GeckoAppShell.getCrashHandlerService() != null ?
                GeckoAppShell.getCrashHandlerService().getName() : null;
        try {
            started = child.start(this, args, extras, flags, crashHandler,
                    prefsPfd, prefMapPfd, ipcPfd, crashPfd, crashAnnotationPfd);
        } catch (final RemoteException e) {
            exception = e;
        }

        if (crashAnnotationPfd != null) {
            crashAnnotationPfd.detachFd();
        }
        if (crashPfd != null) {
            crashPfd.detachFd();
        }
        ipcPfd.detachFd();
        if (prefMapPfd != null) {
            prefMapPfd.detachFd();
        }
        if (prefsPfd != null) {
            prefsPfd.detachFd();
        }

        if (!started) {
            if (retry) {
                Log.e(LOGTAG, "Cannot restart child " + type);
                final StringBuilder builder = new StringBuilder("Cannot restart child.");
                if (prevException != null) {
                    builder.append(" Initial RemoteException: " + prevException.toString());
                }
                if (exception != null) {
                    builder.append(" Second RemoteException: " + exception.toString());
                }
                if (exception == null && prevException == null) {
                    builder.append(" No exceptions thrown; type = " + type);
                }
                throw new RuntimeException(builder.toString());
            }
            Log.w(LOGTAG, "Attempting to kill running child " + type);
            connection.unbind();
            return start(type, args, prefsFd, prefMapFd, ipcFd, crashFd, crashAnnotationFd, /* retry */ true, exception);
        }

        return connection.getPid();
    }

} // GeckoProcessManager
