/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.process;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoNetworkManager;
import org.mozilla.gecko.TelemetryUtils;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.IGeckoEditableChild;
import org.mozilla.gecko.IGeckoEditableParent;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.mozglue.JNIObject;
import org.mozilla.gecko.process.ServiceAllocator.PriorityLevel;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.XPCOMEventTarget;

import org.mozilla.geckoview.GeckoResult;

import android.os.Bundle;
import android.os.DeadObjectException;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import androidx.annotation.NonNull;
import androidx.collection.ArrayMap;
import androidx.collection.ArraySet;
import androidx.collection.SimpleArrayMap;
import android.util.Log;

import java.util.UUID;


public final class GeckoProcessManager extends IProcessManager.Stub {
    private static final String LOGTAG = "GeckoProcessManager";
    private static final GeckoProcessManager INSTANCE = new GeckoProcessManager();
    private static final int INVALID_PID = 0;

    // This id univocally identifies the current process manager instance
    private final String mInstanceId;

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

        @WrapForJNI
        private Selector(@NonNull final GeckoProcessType type) {
            mType = type;
            mPid = INVALID_PID;
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

    private static final class IncompleteChildConnectionException extends RuntimeException {
        public IncompleteChildConnectionException(@NonNull final String msg) {
            super(msg);
        }
    }

    /**
     * Maintains state pertaining to an individual child process. Inheriting from
     * ServiceAllocator.InstanceInfo enables this class to work with ServiceAllocator.
     */
    private static class ChildConnection extends ServiceAllocator.InstanceInfo {
        private IChildProcess mChild;
        private GeckoResult<IChildProcess> mPendingBind;
        private int mPid;

        protected ChildConnection(@NonNull final ServiceAllocator allocator,
                                  @NonNull final GeckoProcessType type,
                                  @NonNull final PriorityLevel initialPriority) {
            super(allocator, type, initialPriority);
            mPid = INVALID_PID;
        }

        public int getPid() {
            XPCOMEventTarget.assertOnLauncherThread();
            if (mChild == null) {
                throw new IncompleteChildConnectionException("Calling ChildConnection.getPid() on an incomplete connection");
            }

            return mPid;
        }

        private GeckoResult<IChildProcess> completeFailedBind(@NonNull final ServiceAllocator.BindException e) {
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
                    throw new ServiceAllocator.BindException("Cannot connect to process");
                }
            } catch (final ServiceAllocator.BindException e) {
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

            unbindService();

            return GeckoResult.fromValue(null);
        }

        @Override
        protected void onBinderConnected(final IBinder service) {
            XPCOMEventTarget.assertOnLauncherThread();

            final IChildProcess child = IChildProcess.Stub.asInterface(service);
            try {
                mPid = child.getPid();
            } catch (final DeadObjectException e) {
                unbindService();

                // mPendingBind might be null if a bind was initiated by the system (eg Service Restart)
                if (mPendingBind != null) {
                    mPendingBind.completeExceptionally(e);
                    mPendingBind = null;
                }

                return;
            } catch (final RemoteException e) {
                throw new RuntimeException(e);
            }

            mChild = child;
            GeckoProcessManager.INSTANCE.mConnections.onBindComplete(this);

            // mPendingBind might be null if a bind was initiated by the system (eg Service Restart)
            if (mPendingBind != null) {
                mPendingBind.complete(mChild);
                mPendingBind = null;
            }
        }

        @Override
        protected void onReleaseResources() {
            XPCOMEventTarget.assertOnLauncherThread();

            // NB: This must happen *before* resetting mPid!
            GeckoProcessManager.INSTANCE.mConnections.removeConnection(this);

            mChild = null;
            mPid = INVALID_PID;
        }
    }

    private static class NonContentConnection extends ChildConnection {
        public NonContentConnection(@NonNull final ServiceAllocator allocator,
                                    @NonNull final GeckoProcessType type) {
            super(allocator, type, PriorityLevel.FOREGROUND);
            if (type == GeckoProcessType.CONTENT) {
                throw new AssertionError("Attempt to create a NonContentConnection as CONTENT");
            }
        }

        protected void onAppForeground() {
            setPriorityLevel(PriorityLevel.FOREGROUND);
        }

        protected void onAppBackground() {
            setPriorityLevel(PriorityLevel.BACKGROUND);
        }
    }

    private static final class SocketProcessConnection extends NonContentConnection {
        private boolean mIsForeground = true;
        private boolean mIsNetworkUp = true;

        public SocketProcessConnection(@NonNull final ServiceAllocator allocator) {
            super(allocator, GeckoProcessType.SOCKET);
            GeckoProcessManager.INSTANCE.mConnections.enableNetworkNotifications();
        }

        public void onNetworkStateChange(final boolean isNetworkUp) {
            mIsNetworkUp = isNetworkUp;
            prioritize();
        }

        @Override
        protected void onAppForeground() {
            mIsForeground = true;
            prioritize();
        }

        @Override
        protected void onAppBackground() {
            mIsForeground = false;
            prioritize();
        }

        private static final PriorityLevel[][] sPriorityStates = initPriorityStates();

        private static PriorityLevel[][] initPriorityStates() {
            final PriorityLevel[][] states = new PriorityLevel[2][2];
            // Background, no network
            states[0][0] = PriorityLevel.IDLE;
            // Background, network
            states[0][1] = PriorityLevel.BACKGROUND;
            // Foreground, no network
            states[1][0] = PriorityLevel.IDLE;
            // Foreground, network
            states[1][1] = PriorityLevel.FOREGROUND;
            return states;
        }

        private void prioritize() {
            final PriorityLevel nextPriority = sPriorityStates[mIsForeground ? 1 : 0][mIsNetworkUp ? 1 : 0];
            setPriorityLevel(nextPriority);
        }
    }

    private static final class ContentConnection extends ChildConnection {
        private static final String TELEMETRY_PROCESS_LIFETIME_HISTOGRAM_NAME = "GV_CONTENT_PROCESS_LIFETIME_MS";

        private TelemetryUtils.UptimeTimer mLifetimeTimer = null;

        public ContentConnection(@NonNull final ServiceAllocator allocator,
                                 @NonNull final PriorityLevel initialPriority) {
            super(allocator, GeckoProcessType.CONTENT, initialPriority);
        }

        @Override
        protected void onBinderConnected(final IBinder service) {
            mLifetimeTimer = new TelemetryUtils.UptimeTimer(TELEMETRY_PROCESS_LIFETIME_HISTOGRAM_NAME);
            super.onBinderConnected(service);
        }

        @Override
        protected void onReleaseResources() {
            if (mLifetimeTimer != null) {
                mLifetimeTimer.stop();
                mLifetimeTimer = null;
            }

            super.onReleaseResources();
        }
    }

    /**
     * This class manages the state surrounding existing connections and their priorities.
     */
    private static final class ConnectionManager extends JNIObject {
        // Connections to non-content processes
        private final ArrayMap<GeckoProcessType, NonContentConnection> mNonContentConnections;
        // Mapping of pid to content process
        private final SimpleArrayMap<Integer, ContentConnection> mContentPids;
        // Set of initialized content process connections
        private final ArraySet<ContentConnection> mContentConnections;
        // Set of bound but uninitialized content connections
        private final ArraySet<ContentConnection> mNonStartedContentConnections;
        // Allocator for service IDs
        private final ServiceAllocator mServiceAllocator;
        private boolean mIsObservingNetwork = false;

        public ConnectionManager() {
            mNonContentConnections = new ArrayMap<GeckoProcessType, NonContentConnection>();
            mContentPids = new SimpleArrayMap<Integer, ContentConnection>();
            mContentConnections = new ArraySet<ContentConnection>();
            mNonStartedContentConnections = new ArraySet<ContentConnection>();
            mServiceAllocator = new ServiceAllocator();

            // Attach to native once JNI is ready.
            if (GeckoThread.isStateAtLeast(GeckoThread.State.JNI_READY)) {
                attachTo(this);
            } else {
                GeckoThread.queueNativeCallUntil(GeckoThread.State.JNI_READY, ConnectionManager.class,
                                                 "attachTo", this);
            }
        }

        private void enableNetworkNotifications() {
            if (mIsObservingNetwork) {
                return;
            }

            mIsObservingNetwork = true;

            // Ensure that GeckoNetworkManager is monitoring network events so that we can
            // prioritize the socket process.
            ThreadUtils.runOnUiThread(() -> {
                GeckoNetworkManager.getInstance().enableNotifications();
            });

            observeNetworkNotifications();
        }

        @WrapForJNI(dispatchTo = "gecko")
        private static native void attachTo(ConnectionManager instance);

        @WrapForJNI(dispatchTo = "gecko")
        private native void observeNetworkNotifications();

        @WrapForJNI(calledFrom = "gecko")
        private void onBackground() {
            XPCOMEventTarget.runOnLauncherThread(() -> onAppBackgroundInternal());
        }

        @WrapForJNI(calledFrom = "gecko")
        private void onForeground() {
            XPCOMEventTarget.runOnLauncherThread(() -> onAppForegroundInternal());
        }

        @WrapForJNI(calledFrom = "gecko")
        private void onNetworkStateChange(final boolean isUp) {
            XPCOMEventTarget.runOnLauncherThread(() -> onNetworkStateChangeInternal(isUp));
        }

        @Override
        protected native void disposeNative();

        private void onAppBackgroundInternal() {
            XPCOMEventTarget.assertOnLauncherThread();

            for (final NonContentConnection conn : mNonContentConnections.values()) {
                conn.onAppBackground();
            }
        }

        private void onAppForegroundInternal() {
            XPCOMEventTarget.assertOnLauncherThread();

            for (final NonContentConnection conn : mNonContentConnections.values()) {
                conn.onAppForeground();
            }
        }

        private void onNetworkStateChangeInternal(final boolean isUp) {
            XPCOMEventTarget.assertOnLauncherThread();

            final SocketProcessConnection conn = (SocketProcessConnection) mNonContentConnections.get(GeckoProcessType.SOCKET);
            if (conn == null) {
                return;
            }

            conn.onNetworkStateChange(isUp);
        }

        private void removeContentConnection(@NonNull final ChildConnection conn) {
            if (!mContentConnections.remove(conn) && !mNonStartedContentConnections.remove(conn)) {
                throw new RuntimeException("Attempt to remove non-registered connection");
            }

            final int pid;

            try {
                pid = conn.getPid();
            } catch (final IncompleteChildConnectionException e) {
                // conn lost its binding before it was able to retrieve its pid. It follows that
                // mContentPids does not have an entry for this connection, so we can just return.
                return;
            }

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
        public void onBindComplete(@NonNull final ChildConnection conn) {
            if (conn.getType() == GeckoProcessType.CONTENT) {
                int pid = conn.getPid();
                if (pid == INVALID_PID) {
                    throw new AssertionError("PID is invalid even though our caller just successfully retrieved it after binding");
                }

                mContentPids.put(Integer.valueOf(pid), (ContentConnection) conn);
            }
        }

        /**
         * Retrieve the ChildConnection for an already running content process.
         */
        private ContentConnection getExistingContentConnection(@NonNull final Selector selector) {
            XPCOMEventTarget.assertOnLauncherThread();
            if (selector.getType() != GeckoProcessType.CONTENT) {
                throw new IllegalArgumentException("Selector is not for content!");
            }

            return mContentPids.get(Integer.valueOf(selector.getPid()));
        }

        /**
         * Unconditionally create a new content connection for the specified priority.
         */
        private ContentConnection getNewContentConnection(@NonNull final PriorityLevel newPriority) {
            final ContentConnection result = new ContentConnection(mServiceAllocator, newPriority);
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
                return getNewContentConnection(PriorityLevel.FOREGROUND);
            }

            final ChildConnection conn = mNonStartedContentConnections.removeAt(mNonStartedContentConnections.size() - 1);
            conn.setPriorityLevel(PriorityLevel.FOREGROUND);
            return conn;
        }

        /**
         * Retrieve or create a new child process for the specified non-content process.
         */
        private ChildConnection getNonContentConnection(@NonNull final GeckoProcessType type) {
            XPCOMEventTarget.assertOnLauncherThread();
            if (type == GeckoProcessType.CONTENT) {
                throw new IllegalArgumentException("Content processes not supported by this method");
            }

            NonContentConnection connection = mNonContentConnections.get(type);
            if (connection == null) {
                if (type == GeckoProcessType.SOCKET) {
                    connection = new SocketProcessConnection(mServiceAllocator);
                } else {
                    connection = new NonContentConnection(mServiceAllocator, type);
                }

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
                final ContentConnection conn = getNewContentConnection(PriorityLevel.BACKGROUND);
                mNonStartedContentConnections.add(conn);
                return conn;
            }

            return getNonContentConnection(type);
        }
    }

    private final ConnectionManager mConnections;

    private GeckoProcessManager() {
        mConnections = new ConnectionManager();
        mInstanceId = UUID.randomUUID().toString();
    }

    public void preload(final GeckoProcessType... types) {
        XPCOMEventTarget.launcherThread().execute(() -> {
            for (final GeckoProcessType type : types) {
                final ChildConnection connection = mConnections.getConnectionForPreload(type);
                connection.bind();
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
    private static void shutdownProcess(final Selector selector) {
        XPCOMEventTarget.assertOnLauncherThread();
        final ChildConnection conn = INSTANCE.mConnections.getExistingConnection(selector);
        if (conn == null) {
            return;
        }

        conn.unbind();
    }

    @WrapForJNI
    private static void setProcessPriority(@NonNull final Selector selector,
                                           @NonNull final PriorityLevel priorityLevel,
                                           final int relativeImportance) {
        XPCOMEventTarget.runOnLauncherThread(() -> {
            final ChildConnection conn = INSTANCE.mConnections.getExistingConnection(selector);
            if (conn == null) {
                return;
            }

            conn.setPriorityLevel(priorityLevel, relativeImportance);
        });
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

        XPCOMEventTarget.runOnLauncherThread(() -> {
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

    private void acceptUnbindFailure(@NonNull final GeckoResult<Void> unbindResult,
                                     @NonNull final GeckoResult<Integer> finalResult,
                                     final RemoteException exception,
                                     @NonNull final GeckoProcessType type,
                                     final boolean isRetry) {
        unbindResult.accept(null, error -> {
            final StringBuilder builder = new StringBuilder("Failed to unbind");
            if (isRetry) {
                builder.append(": ");
            } else {
                builder.append(" before child restart: ");
            }

            builder.append(error.toString());
            if (exception != null) {
                builder.append("; In response to RemoteException: ");
                builder.append(exception.toString());
            }

            builder.append("; Type = ");
            builder.append(type.toString());

            finalResult.completeExceptionally(new RuntimeException(builder.toString()));
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

        int started = IChildProcess.STARTED_FAIL;
        RemoteException exception = null;
        final String crashHandler = GeckoAppShell.getCrashHandlerService() != null ?
                GeckoAppShell.getCrashHandlerService().getName() : null;
        try {
            started = child.start(this, mInstanceId, args, extras, flags, crashHandler,
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

        if (started == IChildProcess.STARTED_OK) {
            result.complete(connection.getPid());
            return;
        } else if (started == IChildProcess.STARTED_BUSY) {
            // This process is owned by a different runtime, so we can't use
            // it. Let's try another process.
            // Note: this strategy is pretty bad, we go through each process in
            // sequence until one works, the multiple runtime case is test-only
            // for now, so that's ok. We can improve on this if we eventually
            // end up needing something fancier.
            Log.w(LOGTAG, "Trying a different process");
            start(result, type, args, extras, flags, prefsFd, prefMapFd, ipcFd,
                    crashFd, crashAnnotationFd, /* isRetry */ false);
            return;
        }

        // Whether retrying or not, we should always unbind connection so that it gets cleaned up.
        final GeckoResult<Void> unbindResult = connection.unbind();

        // We always complete result exceptionally if the unbind fails
        acceptUnbindFailure(unbindResult, result, exception, type, isRetry);

        if (isRetry) {
            // If we've already retried, just assemble an error message and completeExceptionally.
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

            final RuntimeException completionException = new RuntimeException(builder.toString());
            unbindResult.accept(v -> {
                result.completeExceptionally(completionException);
            });
            return;
        }

        // Attempt to retry the connection once we've finished unbinding.
        Log.w(LOGTAG, "Attempting to kill running child " + type.toString());
        final RemoteException captureException = exception;
        unbindResult.accept(v -> {
            start(result, type, args, extras, flags, prefsFd, prefMapFd, ipcFd,
                  crashFd, crashAnnotationFd, /* isRetry */ true, captureException);
        });
    }

} // GeckoProcessManager
