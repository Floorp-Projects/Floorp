/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.process;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.IGeckoEditableChild;
import org.mozilla.gecko.IGeckoEditableParent;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.process.ServiceAllocator.PriorityLevel;
import org.mozilla.gecko.util.XPCOMEventTarget;

import org.mozilla.geckoview.GeckoResult;

import android.os.Bundle;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.os.RemoteException;
import android.support.annotation.NonNull;
import android.support.v4.util.ArraySet;
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

    /**
     * Gecko uses this class to uniquely identify a process managed by GeckoProcessManager.
     */
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

    /**
     * Maintains state pertaining to an individual child process. Inheriting from
     * ServiceAllocator.InstanceInfo enables this class to work with ServiceAllocator.
     */
    private static final class ChildConnection extends ServiceAllocator.InstanceInfo {
        private IChildProcess mChild;
        private GeckoResult<IChildProcess> mPendingBind;
        private int mPid;

        public ChildConnection(@NonNull final ServiceAllocator allocator,
                               @NonNull final GeckoProcessType type,
                               @NonNull final PriorityLevel priority) {
            super(allocator, type, priority);
            mPid = INVALID_PID;
        }

        public ChildConnection(@NonNull final ServiceAllocator allocator,
                               @NonNull final GeckoProcessType type) {
            this(allocator, type, PriorityLevel.FOREGROUND);
        }

        public int getPid() throws RemoteException {
            XPCOMEventTarget.assertOnLauncherThread();
            if (mChild == null) {
                throw new IllegalStateException("Calling ChildConnection.getPid() on an unbound connection");
            }

            if (mPid == INVALID_PID) {
                mPid = mChild.getPid();
            }

            if (mPid == INVALID_PID) {
                throw new RuntimeException("Unable to obtain a valid pid for connection");
            }

            return mPid;
        }

        public int getPidFallible() {
            try {
                return getPid();
            } catch (final Exception e) {
                Log.w(LOGTAG, "Cannot get pid for " + getType().toString(), e);
                return INVALID_PID;
            }
        }

        private String buildLogMsg(@NonNull final String msgStart) {
            final StringBuilder builder = new StringBuilder(msgStart);
            builder.append(" ");
            builder.append(getType().toString());

            int pid = getPidFallible();
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

            if (mChild != null) {
                // Already bound
                return GeckoResult.fromValue(mChild);
            }

            if (mPendingBind != null) {
                // Bind in progress
                return mPendingBind;
            }

            mPendingBind = new GeckoResult<>();
            try {
                if (!bindService()) {
                    return completeFailedBind(new RuntimeException(buildLogMsg("Cannot connect to process")));
                }
            } catch (RuntimeException e) {
                return completeFailedBind(e);
            }

            return mPendingBind;
        }

        public GeckoResult<Void> unbind() {
            XPCOMEventTarget.assertOnLauncherThread();

            if (mPendingBind != null) {
                // We called unbind() while bind() was still pending completion
                return mPendingBind.then(child -> unbind());
            }

            if (mChild == null) {
                // Not bound in the first place
                return GeckoResult.fromValue(null);
            }

            // This could end up using IPC, so do it before we unbind.
            final int pid = getPidFallible();

            doUnbind();

            if (pid != INVALID_PID) {
                Process.killProcess(pid);
            }

            return GeckoResult.fromValue(null);
        }

        private void clearConnectionInfo() {
            // NB: This must happen *before* resetting mPid!
            GeckoProcessManager.INSTANCE.mConnections.removeConnection(this);

            mChild = null;
            mPid = INVALID_PID;
        }

        private void doUnbind() {
            try {
                unbindService();
            } catch (IllegalArgumentException e) {
                // The binding was already dead. That's okay.
            } finally {
                clearConnectionInfo();
            }
        }

        @Override
        protected void onBinderConnected(final IBinder service) {
            XPCOMEventTarget.assertOnLauncherThread();

            mChild = IChildProcess.Stub.asInterface(service);

            // mPendingBind might be null if a bind was initiated by the system (eg Service Restart)
            if (mPendingBind != null) {
                mPendingBind.complete(mChild);
                mPendingBind = null;
            }
        }

        @Override
        protected void onBinderConnectionLost() {
            XPCOMEventTarget.assertOnLauncherThread();

            // The binding has lost its connection, but the binding itself might still be active.
            // Gecko itself will request a process restart, so here we attempt to unbind so that
            // Android does not try to automatically restart and reconnect the service.
            doUnbind();
        }
    }

    /**
     * This class manages the state surrounding existing connections and their priorities.
     */
    private static final class ConnectionManager {
        // Connections to non-content processes
        private final SimpleArrayMap<GeckoProcessType, ChildConnection> mNonContentConnections;
        // Mapping of pid to content process
        private final SimpleArrayMap<Integer, ChildConnection> mContentPids;
        // Set of initialized content process connections
        private final ArraySet<ChildConnection> mContentConnections;
        // Set of bound but uninitialized content connections
        private final ArraySet<ChildConnection> mNonStartedContentConnections;
        // Allocator for service IDs
        private final ServiceAllocator mServiceAllocator;

        public ConnectionManager() {
            mNonContentConnections = new SimpleArrayMap<GeckoProcessType, ChildConnection>();
            mContentPids = new SimpleArrayMap<Integer, ChildConnection>();
            mContentConnections = new ArraySet<ChildConnection>();
            mNonStartedContentConnections = new ArraySet<ChildConnection>();
            mServiceAllocator = new ServiceAllocator();
        }

        private void removeContentConnection(@NonNull final ChildConnection conn) {
            if (!mContentConnections.remove(conn) && !mNonStartedContentConnections.remove(conn)) {
                throw new RuntimeException("Attempt to remove non-registered connection");
            }

            final int pid = conn.getPidFallible();
            if (pid == INVALID_PID) {
                return;
            }

            final ChildConnection removed = mContentPids.remove(Integer.valueOf(pid));
            if (removed != null && removed != conn) {
                throw new RuntimeException("Integrity error - connection mismatch for pid " + Integer.toString(pid));
            }
        }

        public void removeConnection(@NonNull final ChildConnection conn) {
            XPCOMEventTarget.assertOnLauncherThread();

            if (conn.getType() == GeckoProcessType.CONTENT) {
                removeContentConnection(conn);
                return;
            }

            final ChildConnection removed = mNonContentConnections.remove(conn.getType());
            if (removed != conn) {
                throw new RuntimeException("Integrity error - connection mismatch for process type " + conn.getType().toString());
            }
        }

        /**
         * Saves any state information that was acquired upon start completion.
         */
        public void onStartComplete(@NonNull final ChildConnection conn) throws RemoteException {
            if (conn.getType() == GeckoProcessType.CONTENT) {
                int pid = conn.getPid();
                mContentPids.put(Integer.valueOf(pid), conn);
            }
        }

        /**
         * Retrieve the ChildConnection for an already running content process.
         */
        private ChildConnection getExistingContentConnection(@NonNull final Selector selector) {
            XPCOMEventTarget.assertOnLauncherThread();
            if (selector.getType() != GeckoProcessType.CONTENT) {
                throw new RuntimeException("Selector is not for content!");
            }

            return mContentPids.get(Integer.valueOf(selector.getPid()));
        }

        /**
         * Unconditionally create a new content connection for the specified priority.
         */
        private ChildConnection getNewContentConnection(@NonNull final PriorityLevel newPriority) {
            final ChildConnection result = new ChildConnection(mServiceAllocator, GeckoProcessType.CONTENT, newPriority);
            mContentConnections.add(result);

            return result;
        }

        /**
         * Retrieve the ChildConnection for an already running child process of any type.
         */
        public ChildConnection getExistingConnection(@NonNull final Selector selector) {
            XPCOMEventTarget.assertOnLauncherThread();

            final GeckoProcessType type = selector.getType();

            if (type == GeckoProcessType.CONTENT) {
                return getExistingContentConnection(selector);
            }

            return mNonContentConnections.get(type);
        }

        /**
         * Retrieve a ChildConnection for a content process for the purposes of starting. If there
         * are any preloaded content processes already running, we will use one of those.
         * Otherwise we will allocate a new ChildConnection.
         */
        private ChildConnection getContentConnectionForStart() {
            XPCOMEventTarget.assertOnLauncherThread();

            if (mNonStartedContentConnections.isEmpty()) {
                // Only FOREGROUND supported right now
                return getNewContentConnection(PriorityLevel.FOREGROUND);
            }

            return mNonStartedContentConnections.removeAt(mNonStartedContentConnections.size() - 1);
        }

        /**
         * Retrieve or create a new child process for the specified non-content process.
         */
        private ChildConnection getNonContentConnection(@NonNull final GeckoProcessType type) {
            XPCOMEventTarget.assertOnLauncherThread();
            if (type == GeckoProcessType.CONTENT) {
                throw new IllegalArgumentException("Content processes not supported by this method");
            }

            ChildConnection connection = mNonContentConnections.get(type);
            if (connection == null) {
                connection = new ChildConnection(mServiceAllocator, type);
                mNonContentConnections.put(type, connection);
            }
            return connection;
        }

        /**
         * Retrieve a ChildConnection for the purposes of starting a new child process.
         */
        public ChildConnection getConnectionForStart(@NonNull final GeckoProcessType type) {
            if (type == GeckoProcessType.CONTENT) {
                return getContentConnectionForStart();
            }

            return getNonContentConnection(type);
        }

        /**
         * Retrieve a ChildConnection for the purposes of preloading a new child process.
         */
        public ChildConnection getConnectionForPreload(@NonNull final GeckoProcessType type) {
            if (type == GeckoProcessType.CONTENT) {
                final ChildConnection conn = getNewContentConnection(PriorityLevel.FOREGROUND);
                mNonStartedContentConnections.add(conn);
                return conn;
            }

            return getNonContentConnection(type);
        }
    }

    private final ConnectionManager mConnections;

    private GeckoProcessManager() {
        mConnections = new ConnectionManager();
    }

    public void preload(final GeckoProcessType... types) {
        XPCOMEventTarget.launcherThread().execute(() -> {
            for (final GeckoProcessType type : types) {
                final ChildConnection connection = mConnections.getConnectionForPreload(type);
                connection.bind().accept(child -> {
                    try {
                        child.getPid();
                    } catch (final RemoteException e) {
                        Log.e(LOGTAG, "Cannot get pid for " + type.toString(), e);
                        return;
                    }
                });
            }
        });
    }

    public void crashChild(@NonNull final Selector selector) {
        XPCOMEventTarget.launcherThread().execute(() -> {
            final ChildConnection conn = mConnections.getExistingConnection(selector);
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
        final ChildConnection conn = INSTANCE.mConnections.getExistingConnection(selector);
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

        XPCOMEventTarget.launcherThread().execute(() -> {
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

        final ChildConnection connection = mConnections.getConnectionForStart(type);
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
            try {
                mConnections.onStartComplete(connection);
                result.complete(connection.getPid());
                return;
            } catch (final RemoteException e) {
                exception = e;
            } catch (final Exception e) {
                Log.e(LOGTAG, "ChildConnection.getPid() exception: ", e);
            }

            // If we don't have a valid pid, then fall through to our error handling code that will
            // attempt to retry the launch.
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
