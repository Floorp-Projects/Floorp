/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.process;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.IGeckoEditableChild;
import org.mozilla.gecko.IGeckoEditableParent;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.util.XPCOMEventTarget;

import org.mozilla.geckoview.GeckoResult;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.os.RemoteException;
import android.support.annotation.NonNull;
import android.support.v4.util.SimpleArrayMap;
import android.util.Log;

public final class GeckoProcessManager extends IProcessManager.Stub {
    private static final String LOGTAG = "GeckoProcessManager";
    private static final GeckoProcessManager INSTANCE = new GeckoProcessManager();
    private static final int INVALID_PID = 0;

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

    public static final class Selector {
        private final GeckoProcessType mType;
        private final int mPid;

        @WrapForJNI
        private Selector(@NonNull final GeckoProcessType type, final int pid) {
            if (pid == INVALID_PID) {
                throw new RuntimeException("Invalid PID");
            }

            mType = type;
            mPid = pid;
        }

        public GeckoProcessType getType() {
            return mType;
        }

        public int getPid() {
            return mPid;
        }

        @Override
        public boolean equals(final Object obj) {
            if (obj == null) {
                return false;
            }

            if (obj == ((Object)this)) {
                return true;
            }

            final Selector other = (Selector) obj;
            return mType == other.mType && mPid == other.mPid;
        }

        @Override
        public int hashCode() {
            return 31 * mType.hashCode() + mPid;
        }
    }

    private static final class ChildConnection implements ServiceConnection,
                                                          IBinder.DeathRecipient {
        private final GeckoProcessType mType;
        private IChildProcess mChild;
        private GeckoResult<IChildProcess> mPendingBind;
        private GeckoResult<Void> mPendingUnbind;
        private int mPid;

        public ChildConnection(@NonNull final GeckoProcessType type) {
            mType = type;
        }

        public int getPid() {
            XPCOMEventTarget.assertOnLauncherThread();
            if ((mPid == INVALID_PID) && (mChild != null)) {
                try {
                    mPid = mChild.getPid();
                } catch (final RemoteException e) {
                    Log.e(LOGTAG, "Cannot get pid for " + mType.toString(), e);
                }
            }
            return mPid;
        }

        private String buildLogMsg(@NonNull final String msgStart) {
            final StringBuilder builder = new StringBuilder(msgStart);
            builder.append(" ");
            builder.append(mType.toString());

            int pid = getPid();
            if (pid != INVALID_PID) {
                builder.append(" with pid ");
                builder.append(pid);
            }

            return builder.toString();
        }

        private GeckoResult<IChildProcess> completeFailedBind(@NonNull final Throwable e) {
            XPCOMEventTarget.assertOnLauncherThread();
            Log.e(LOGTAG, "Failed bind", e);

            if (mPendingBind == null) {
                throw new IllegalStateException("Bind failed with null mPendingBind");
            }

            final GeckoResult<IChildProcess> bindResult = mPendingBind;
            mPendingBind = null;
            unbind().accept(v -> bindResult.completeExceptionally(e));
            return bindResult;
        }

        public GeckoResult<IChildProcess> bind() {
            XPCOMEventTarget.assertOnLauncherThread();

            if (mPendingUnbind != null) {
                // Unbind is in progress; we cannot proceed until that has completed
                return mPendingUnbind.then(v -> {
                    return bind();
                });
            }
            if (mChild != null) {
                // Already bound
                return GeckoResult.fromValue(mChild);
            }
            if (mPendingBind != null) {
                // Bind in progress
                return mPendingBind;
            }

            final Context context = GeckoAppShell.getApplicationContext();
            final Intent intent = new Intent();
            intent.setClassName(context,
                                GeckoServiceChildProcess.class.getName() + '$' + mType.toString());

            mPendingBind = new GeckoResult<>();
            try {
                if (!context.bindService(intent, this, Context.BIND_AUTO_CREATE | Context.BIND_IMPORTANT)) {
                    return completeFailedBind(new RuntimeException(buildLogMsg("Cannot connect to process")));
                }
            } catch (RuntimeException e) {
                return completeFailedBind(e);
            }

            return mPendingBind;
        }

        public GeckoResult<Void> unbind() {
            XPCOMEventTarget.assertOnLauncherThread();

            if (mPendingUnbind != null) {
                // unbind already in progress
                return mPendingUnbind;
            }

            // This could end up using IPC, so do it before we unbind.
            final int pid = getPid();

            if (mPendingBind != null) {
                // We called unbind() while bind() was still pending completion
                return mPendingBind.then(child -> unbind());
            }

            if (mChild != null) {
                mPendingUnbind = new GeckoResult<>();
                final Context context = GeckoAppShell.getApplicationContext();
                try {
                    context.unbindService(this);
                } catch (IllegalArgumentException e) {
                    mChild = null;
                    mPid = INVALID_PID;
                    final GeckoResult<Void> unbindResult = mPendingUnbind;
                    mPendingUnbind = null;
                    // The caller can't do anything about this, just complete
                    unbindResult.complete(null);
                    return unbindResult;
                }
            }

            if (pid == INVALID_PID) {
                final GeckoResult<Void> unbindResult = mPendingUnbind;
                mPendingUnbind = null;

                if (unbindResult == null) {
                    return GeckoResult.fromValue(null);
                }

                unbindResult.complete(null);
                return unbindResult;
            }

            Process.killProcess(pid);

            return mPendingUnbind;
        }

        private void completeServiceConnect(final IBinder service) {
            XPCOMEventTarget.assertOnLauncherThread();

            try {
                service.linkToDeath(this, 0);
            } catch (final RemoteException e) {
                Log.e(LOGTAG, buildLogMsg("Cannot link to death for"), e);
            }

            mChild = IChildProcess.Stub.asInterface(service);

            // mPendingBind might be null if a bind was initiated by the system (eg Service Restart)
            if (mPendingBind != null) {
                mPendingBind.complete(mChild);
                mPendingBind = null;
            }
        }

        private void completeServiceDisconnect() {
            XPCOMEventTarget.assertOnLauncherThread();
            mChild = null;
            mPid = INVALID_PID;
            if (mPendingUnbind != null) {
                mPendingUnbind.complete(null);
                mPendingUnbind = null;
            }
        }

        private void onBinderDeath() {
            XPCOMEventTarget.assertOnLauncherThread();
            Log.i(LOGTAG, buildLogMsg("Binder died for"));

            if (mChild != null) {
                mChild = null;
                mPid = INVALID_PID;

                try {
                    GeckoAppShell.getApplicationContext().unbindService(this);
                } catch (IllegalArgumentException e) {
                    if (mPendingUnbind != null) {
                        // completeServiceConnect will never be called because we were
                        // never considered to be bound. This may be indicative of multiple
                        // GeckoRuntimes in a single logical application binding to the same
                        // service. We complete mPendingUnbind since this condition might be
                        // recoverable.
                        Log.w(LOGTAG, "Attempt to unbind a service that is not currently bound");
                        mPendingUnbind.complete(null);
                        mPendingUnbind = null;
                    }
                }
            }
        }

        @Override
        public void onServiceConnected(final ComponentName name,
                                       final IBinder service) {
            XPCOMEventTarget.launcherThread().dispatch(() -> {
                completeServiceConnect(service);
            });
        }

        @Override
        public void onServiceDisconnected(final ComponentName name) {
            XPCOMEventTarget.launcherThread().dispatch(() -> {
                completeServiceDisconnect();
            });
        }

        @Override
        public void binderDied() {
            XPCOMEventTarget.launcherThread().dispatch(() -> {
                onBinderDeath();
            });
        }
    }

    private final SimpleArrayMap<GeckoProcessType, ChildConnection> mConnections;

    private GeckoProcessManager() {
        mConnections = new SimpleArrayMap<GeckoProcessType, ChildConnection>();
    }

    private ChildConnection getConnection(final GeckoProcessType type) {
        XPCOMEventTarget.assertOnLauncherThread();

        ChildConnection connection = mConnections.get(type);
        if (connection == null) {
            connection = new ChildConnection(type);
            mConnections.put(type, connection);
        }
        return connection;
    }

    public void preload(final GeckoProcessType... types) {
        XPCOMEventTarget.launcherThread().dispatch(() -> {
            for (final GeckoProcessType type : types) {
                final ChildConnection connection = getConnection(type);
                connection.bind().accept(child -> {
                    try {
                        child.getPid();
                    } catch (final RemoteException e) {
                        Log.e(LOGTAG, "Cannot get pid for " + type.toString(), e);
                    }
                });
            }
        });
    }

    public void crashChild() {
        XPCOMEventTarget.launcherThread().dispatch(() -> {
            final ChildConnection conn = mConnections.get(GeckoProcessType.CONTENT);
            if (conn == null) {
                return;
            }

            conn.bind().accept(proc -> {
                try {
                    proc.crash();
                } catch (RemoteException e) {
                }
            });
        });
    }

    @WrapForJNI
    private static void markAsDead(final Selector selector) {
        XPCOMEventTarget.assertOnLauncherThread();
        final ChildConnection conn = INSTANCE.mConnections.get(selector.getType());
        if (conn == null) {
            return;
        }

        conn.unbind();
    }

    @WrapForJNI
    private static GeckoResult<Integer> start(final GeckoProcessType type,
                                              final String[] args,
                                              final int prefsFd,
                                              final int prefMapFd,
                                              final int ipcFd,
                                              final int crashFd,
                                              final int crashAnnotationFd) {
        final GeckoResult<Integer> result = new GeckoResult<>();
        final Bundle extras = GeckoThread.getActiveExtras();
        final int flags = filterFlagsForChild(GeckoThread.getActiveFlags());

        XPCOMEventTarget.launcherThread().dispatch(() -> {
            INSTANCE.start(result, type, args, extras, flags, prefsFd,
                           prefMapFd, ipcFd, crashFd, crashAnnotationFd,
                           /* isRetry */ false);
        });

        return result;
    }

    private static int filterFlagsForChild(final int flags) {
        return flags & GeckoThread.FLAG_ENABLE_NATIVE_CRASHREPORTER;
    }

    private void start(final GeckoResult<Integer> result, final GeckoProcessType type,
                       final String[] args, final Bundle extras, final int flags,
                       final int prefsFd, final int prefMapFd, final int ipcFd,
                       final int crashFd, final int crashAnnotationFd,
                       final boolean isRetry) {
        start(result, type, args, extras, flags, prefsFd, prefMapFd, ipcFd,
              crashFd, crashAnnotationFd, isRetry, /* prevException */ null);
    }

    private void start(final GeckoResult<Integer> result, final GeckoProcessType type,
                       final String[] args, final Bundle extras, final int flags,
                       final int prefsFd, final int prefMapFd, final int ipcFd,
                       final int crashFd, final int crashAnnotationFd,
                       final boolean isRetry,
                       final RemoteException prevException) {
        XPCOMEventTarget.assertOnLauncherThread();

        final ChildConnection connection = getConnection(type);
        final GeckoResult<IChildProcess> childResult = connection.bind();

        childResult.accept(childProcess -> {
            start(result, connection, childProcess, type, args, extras,
                  flags, prefsFd, prefMapFd, ipcFd, crashFd,
                  crashAnnotationFd, isRetry, prevException);
        }, error -> {
                final StringBuilder builder = new StringBuilder("Cannot bind child process: ");
                builder.append(error.toString());
                if (prevException != null) {
                    builder.append("; Previous exception: ");
                    builder.append(prevException.toString());
                }

                builder.append("; Type: ");
                builder.append(type.toString());

                result.completeExceptionally(new RuntimeException(builder.toString()));
            });
    }

    private void start(final GeckoResult<Integer> result,
                       final ChildConnection connection,
                       final IChildProcess child,
                       final GeckoProcessType type, final String[] args,
                       final Bundle extras, final int flags,
                       final int prefsFd, final int prefMapFd,
                       final int ipcFd, final int crashFd,
                       final int crashAnnotationFd, final boolean isRetry,
                       final RemoteException prevException) {
        XPCOMEventTarget.assertOnLauncherThread();

        final ParcelFileDescriptor prefsPfd =
                (prefsFd >= 0) ? ParcelFileDescriptor.adoptFd(prefsFd) : null;
        final ParcelFileDescriptor prefMapPfd =
                (prefMapFd >= 0) ? ParcelFileDescriptor.adoptFd(prefMapFd) : null;
        final ParcelFileDescriptor ipcPfd = ParcelFileDescriptor.adoptFd(ipcFd);
        final ParcelFileDescriptor crashPfd =
                (crashFd >= 0) ? ParcelFileDescriptor.adoptFd(crashFd) : null;
        final ParcelFileDescriptor crashAnnotationPfd =
                (crashAnnotationFd >= 0) ? ParcelFileDescriptor.adoptFd(crashAnnotationFd) : null;

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

        if (started) {
            result.complete(connection.getPid());
            return;
        }

        if (isRetry) {
            Log.e(LOGTAG, "Cannot restart child " + type.toString());
            final StringBuilder builder = new StringBuilder("Cannot restart child.");
            if (prevException != null) {
                builder.append(" Initial RemoteException: ");
                builder.append(prevException.toString());
            }
            if (exception != null) {
                builder.append(" Second RemoteException: ");
                builder.append(exception.toString());
            }
            if (exception == null && prevException == null) {
                builder.append(" No exceptions thrown; type = ");
                builder.append(type.toString());
            }
            result.completeExceptionally(new RuntimeException(builder.toString()));
            return;
        }

        final RemoteException captureException = exception;
        Log.w(LOGTAG, "Attempting to kill running child " + type.toString());
        connection.unbind().accept(v -> {
            start(result, type, args, extras, flags, prefsFd, prefMapFd, ipcFd,
                  crashFd, crashAnnotationFd, /* isRetry */ true, captureException);
        }, error -> {
                final StringBuilder builder = new StringBuilder("Failed to unbind before child restart: ");
                builder.append(error.toString());
                if (captureException != null) {
                    builder.append("; In response to RemoteException: ");
                    builder.append(captureException.toString());
                }

                builder.append("; Type = ");
                builder.append(type.toString());

                result.completeExceptionally(new RuntimeException(builder.toString()));
            });
    }

} // GeckoProcessManager
