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

    private static final class ChildConnection implements ServiceConnection,
                                                          IBinder.DeathRecipient {
        private final String mType;
        private IChildProcess mChild;
        private GeckoResult<IChildProcess> mPendingBind;
        private GeckoResult<Void> mPendingUnbind;
        private int mPid;

        public ChildConnection(final String type) {
            mType = type;
        }

        public int getPid() {
            XPCOMEventTarget.assertOnLauncherThread();
            if ((mPid == INVALID_PID) && (mChild != null)) {
                try {
                    mPid = mChild.getPid();
                } catch (final RemoteException e) {
                    Log.e(LOGTAG, "Cannot get pid for " + mType, e);
                }
            }
            return mPid;
        }

        private String buildLogMsg(@NonNull final String msgStart) {
            final StringBuilder builder = new StringBuilder(msgStart);
            builder.append(" ");
            builder.append(mType);

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
                                GeckoServiceChildProcess.class.getName() + '$' + mType);

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

    private final SimpleArrayMap<String, ChildConnection> mConnections;

    private GeckoProcessManager() {
        mConnections = new SimpleArrayMap<String, ChildConnection>();
    }

    private ChildConnection getConnection(final String type) {
        XPCOMEventTarget.assertOnLauncherThread();

        ChildConnection connection = mConnections.get(type);
        if (connection == null) {
            connection = new ChildConnection(type);
            mConnections.put(type, connection);
        }
        return connection;
    }

    public void preload(final String... types) {
        XPCOMEventTarget.launcherThread().dispatch(() -> {
            for (final String type : types) {
                final ChildConnection connection = getConnection(type);
                connection.bind().accept(child -> {
                    try {
                        child.getPid();
                    } catch (final RemoteException e) {
                        Log.e(LOGTAG, "Cannot get pid for " + type, e);
                    }
                });
            }
        });
    }

    public void crashChild() {
        XPCOMEventTarget.launcherThread().dispatch(() -> {
            final ChildConnection conn = mConnections.get("tab");
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
    private static GeckoResult<Integer> start(final String type,
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

    private void start(final GeckoResult<Integer> result, final String type,
                       final String[] args, final Bundle extras, final int flags,
                       final int prefsFd, final int prefMapFd, final int ipcFd,
                       final int crashFd, final int crashAnnotationFd,
                       final boolean isRetry) {
        start(result, type, args, extras, flags, prefsFd, prefMapFd, ipcFd,
              crashFd, crashAnnotationFd, isRetry, /* prevException */ null);
    }

    private void start(final GeckoResult<Integer> result, final String type,
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
                builder.append(type);

                result.completeExceptionally(new RuntimeException(builder.toString()));
            });
    }

    private void start(final GeckoResult<Integer> result,
                       final ChildConnection connection,
                       final IChildProcess child,
                       final String type, final String[] args,
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
            Log.e(LOGTAG, "Cannot restart child " + type);
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
                builder.append(type);
            }
            result.completeExceptionally(new RuntimeException(builder.toString()));
            return;
        }

        final RemoteException captureException = exception;
        Log.w(LOGTAG, "Attempting to kill running child " + type);
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
                builder.append(type);

                result.completeExceptionally(new RuntimeException(builder.toString()));
            });
    }

} // GeckoProcessManager
