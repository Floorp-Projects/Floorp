/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.process;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.util.IXPCOMEventTarget;
import org.mozilla.gecko.util.XPCOMEventTarget;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.support.annotation.NonNull;

import java.lang.reflect.Method;
import java.util.BitSet;
import java.util.concurrent.Executor;

/* package */ final class ServiceAllocator {
    private static final int MAX_NUM_ISOLATED_CONTENT_SERVICES = 50;

    private static final Method sBindIsolatedService = resolveBindIsolatedService();
    private static final Method sBindServiceWithExecutor = resolveBindServiceWithExecutor();

    /**
     * Possible priority levels that are available to child services. Each one maps to a flag that
     * is passed into Context.bindService().
     */
    public static enum PriorityLevel {
        FOREGROUND (Context.BIND_IMPORTANT),
        BACKGROUND (0),
        IDLE (Context.BIND_WAIVE_PRIORITY);

        private final int mAndroidFlag;

        private PriorityLevel(final int androidFlag) {
            mAndroidFlag = androidFlag;
        }

        public int getAndroidFlag() {
            return mAndroidFlag;
        }
    }

    /**
     * Abstract class that holds the essential per-service data that is required to work with
     * ServiceAllocator. ServiceAllocator clients should extend this class when implementing their
     * per-service connection objects.
     */
    public static abstract class InstanceInfo implements ServiceConnection {
        private final ServiceAllocator mAllocator;
        private final GeckoProcessType mType;
        private final Integer mId;
        // Priority level is not yet adjustable, so mPriority is final for now
        private final PriorityLevel mPriority;

        protected InstanceInfo(@NonNull final ServiceAllocator allocator, @NonNull final GeckoProcessType type,
                               @NonNull final PriorityLevel priority) {
            mAllocator = allocator;
            mType = type;
            mId = mAllocator.allocate(type);
            mPriority = priority;
        }

        public PriorityLevel getPriorityLevel() {
            return mPriority;
        }

        /**
         * Only content services have unique IDs. This method throws if called for a non-content
         * service type.
         */
        public int getId() {
            if (mId == null) {
                throw new RuntimeException("This service does not have a unique id");
            }

            return mId.intValue();
        }

        /**
         * This method is infallible and returns an empty string for non-content services.
         */
        private String getIdAsString() {
            return mId == null ? "" : mId.toString();
        }

        public boolean isContent() {
            return mType == GeckoProcessType.CONTENT;
        }

        public GeckoProcessType getType() {
            return mType;
        }

        protected boolean bindService() {
            return mAllocator.bindService(this);
        }

        protected void unbindService() {
            mAllocator.unbindService(this);
        }

        /**
         * This implementation of ServiceConnection.onServiceConnected simply bounces the
         * connection notification over to the launcher thread (if it is not already on it).
         */
        @Override
        public final void onServiceConnected(final ComponentName name,
                                             final IBinder service) {
            final IXPCOMEventTarget launcherThread = XPCOMEventTarget.launcherThread();
            if (launcherThread.isOnCurrentThread()) {
                // If we were able to specify an Executor during binding then we are already on
                // the launcher thread; there is no reason to bounce through its event queue.
                onBinderConnected(service);
                return;
            }

            launcherThread.execute(() -> {
                onBinderConnected(service);
            });
        }

        /**
         * This implementation of ServiceConnection.onServiceDisconnected simply bounces the
         * disconnection notification over to the launcher thread (if it is not already on it).
         */
        @Override
        public final void onServiceDisconnected(final ComponentName name) {
            final IXPCOMEventTarget launcherThread = XPCOMEventTarget.launcherThread();
            if (launcherThread.isOnCurrentThread()) {
                // If we were able to specify an Executor during binding then we are already on
                // the launcher thread; there is no reason to bounce through its event queue.
                onBinderConnectionLost();
                return;
            }

            launcherThread.execute(() -> {
                onBinderConnectionLost();
            });
        }

        /**
         * Called on the launcher thread to inform the client that the service's Binder has been
         * connected. This method is named differently from its ServiceConnection counterpart for
         * the sake of clarity.
         */
        protected abstract void onBinderConnected(@NonNull final IBinder service);

        /**
         * Called on the launcher thread to inform the client that the service's Binder has been
         * lost. This method is named differently from its ServiceConnection counterpart for the
         * sake of clarity. Note that this method is *not* called during a clean unbind.
         */
        protected abstract void onBinderConnectionLost();
    }

    private interface ContentAllocationPolicy {
        /**
         * Bind a new content service.
         */
        boolean bindService(Context context, InstanceInfo info);

        /**
         * Allocate an unused service ID for use by the caller.
         * @return The new service id.
         */
        int allocate();

        /**
         * Release a previously used service ID.
         * @param id The service id being released.
         */
        void release(final int id);
    }

    /**
     * This policy is intended for Android versions &lt; 10, as well as for content process services
     * that are not defined as isolated processes. In this case, the number of possible content
     * service IDs has a fixed upper bound, so we use a BitSet to manage their allocation.
     */
    private static final class DefaultContentPolicy implements ContentAllocationPolicy {
        private final int mMaxNumSvcs;
        private final BitSet mAllocator;

        public DefaultContentPolicy() {
            mMaxNumSvcs = getContentServiceCount();
            mAllocator = new BitSet(mMaxNumSvcs);
        }

        /**
         * This implementation of bindService uses the default implementation as provided by
         * ServiceAllocator.
         */
        @Override
        public boolean bindService(@NonNull final Context context, @NonNull final InstanceInfo info) {
            return ServiceAllocator.bindServiceDefault(context, ServiceAllocator.getSvcClassNameDefault(info), info);
        }

        @Override
        public int allocate() {
            final int next = mAllocator.nextClearBit(0);
            if (next >= mMaxNumSvcs) {
                throw new RuntimeException("No more content services available");
            }

            mAllocator.set(next);
            return next;
        }

        @Override
        public void release(final int id) {
            if (!mAllocator.get(id)) {
                throw new IllegalStateException("Releasing an unallocated id!");
            }

            mAllocator.clear(id);
        }

        /**
         * @return The number of content services defined in our manifest.
         */
        private static int getContentServiceCount() {
            return ServiceUtils.getServiceCount(GeckoAppShell.getApplicationContext(), GeckoProcessType.CONTENT);
        }
    }

    /**
     * This policy is intended for Android versions &gt;= 10 when our content process services
     * are defined in our manifest as having isolated processes. Since isolated services share a
     * single service definition, there is no longer an Android-induced hard limit on the number of
     * content processes that may be started. We simply use a monotonically-increasing counter to
     * generate unique instance IDs in this case.
     */
    private static final class IsolatedContentPolicy implements ContentAllocationPolicy {
        private int mNextIsolatedSvcId = 0;
        private int mCurNumIsolatedSvcs = 0;

        /**
         * This implementation of bindService uses the isolated bindService implementation as
         * provided by ServiceAllocator.
         */
        @Override
        public boolean bindService(@NonNull final Context context, @NonNull final InstanceInfo info) {
            return ServiceAllocator.bindServiceIsolated(context, ServiceUtils.buildIsolatedSvcName(info.getType()), info);
        }

        /**
         * We generate a new instance ID simply by incrementing a counter. We do track how many
         * content services are currently active for the purposes of maintaining the configured
         * limit on number of simulatneous content processes.
         */
        @Override
        public int allocate() {
            if (mCurNumIsolatedSvcs >= MAX_NUM_ISOLATED_CONTENT_SERVICES) {
                throw new RuntimeException("No more content services available");
            }

            ++mCurNumIsolatedSvcs;
            return mNextIsolatedSvcId++;
        }

        /**
         * Just drop the count of active services.
         */
        @Override
        public void release(final int id) {
            if (mCurNumIsolatedSvcs <= 0) {
                throw new IllegalStateException("Releasing an unallocated id");
            }

            --mCurNumIsolatedSvcs;
        }
    }

    /**
     * The policy used for allocating content processes.
     */
    private ContentAllocationPolicy mContentAllocPolicy = null;

    /**
     * Clients should call this method to bind their services. This method automagically does the
     * right things depending on the state of info.
     * @param info The InstanceInfo-derived object that contains essential information for setting
     * up the child service.
     */
    public boolean bindService(@NonNull final InstanceInfo info) {
        XPCOMEventTarget.assertOnLauncherThread();
        final Context context = GeckoAppShell.getApplicationContext();
        if (!info.isContent()) {
            // Non-content services just use standard binding.
            return bindServiceDefault(context, getSvcClassNameDefault(info), info);
        }

        // Content services defer to the alloc policy to determine how to bind.
        return mContentAllocPolicy.bindService(context, info);
    }

    /**
     * Unbinds the service described by |info| and releases its unique ID.
     */
    public void unbindService(@NonNull final InstanceInfo info) {
        XPCOMEventTarget.assertOnLauncherThread();
        final Context context = GeckoAppShell.getApplicationContext();
        try {
            context.unbindService(info);
        } finally {
            release(info);
        }
    }

    /**
     * Allocate a service ID.
     * @param type The type of service.
     * @return Integer encapsulating the service ID, or null if no ID is necessary.
     */
    private Integer allocate(@NonNull final GeckoProcessType type) {
        XPCOMEventTarget.assertOnLauncherThread();
        if (type != GeckoProcessType.CONTENT) {
            // No unique id necessary
            return null;
        }

        // Lazy initialization of mContentAllocPolicy to ensure that it is constructed on the
        // launcher thread.
        if (mContentAllocPolicy == null) {
            if (canBindIsolated(GeckoProcessType.CONTENT)) {
                mContentAllocPolicy = new IsolatedContentPolicy();
            } else {
                mContentAllocPolicy = new DefaultContentPolicy();
            }
        }

        return Integer.valueOf(mContentAllocPolicy.allocate());
    }

    /**
     * Free a defunct service's ID if necessary.
     * @param info The InstanceInfo-derived object that contains essential information for tearing
     * down the child service.
     */
    private void release(@NonNull final InstanceInfo info) {
        XPCOMEventTarget.assertOnLauncherThread();
        if (!info.isContent()) {
            return;
        }

        mContentAllocPolicy.release(info.getId());
    }

    /**
     * Find out whether the desired service type is defined in our manifest as having an isolated
     * process.
     * @param type Service type to query
     * @return true if this service type may use isolated binding, otherwise false.
     */
    private static boolean canBindIsolated(@NonNull final GeckoProcessType type) {
        if (sBindIsolatedService == null) {
            return false;
        }

        final Context context = GeckoAppShell.getApplicationContext();
        final int svcFlags = ServiceUtils.getServiceFlags(context, type);
        return (svcFlags & ServiceInfo.FLAG_ISOLATED_PROCESS) != 0;
    }

    /**
     * Convert PriorityLevel into the flags argument to Context.bindService() et al
     */
    private static int getAndroidFlags(@NonNull final PriorityLevel priority) {
        return Context.BIND_AUTO_CREATE | priority.getAndroidFlag();
    }

    /**
     * Obtain the class name to use for service binding in the default (ie, non-isolated) case.
     */
    private static String getSvcClassNameDefault(@NonNull final InstanceInfo info) {
        return ServiceUtils.buildSvcName(info.getType(), info.getIdAsString());
    }

    /**
     * Wrapper for bindService() that utilizes the Context.bindService() overload that accepts an
     * Executor argument, when available. Otherwise it falls back to the legacy overload.
     */
    private static boolean bindServiceDefault(@NonNull final Context context, @NonNull final String svcClassName, @NonNull final InstanceInfo info) {
        final Intent intent = new Intent();
        intent.setClassName(context, svcClassName);

        if (sBindServiceWithExecutor != null) {
            return bindServiceWithExecutor(context, intent, info);
        }

        return context.bindService(intent, info, getAndroidFlags(info.getPriorityLevel()));
    }

    /**
     * Wrapper that calls the reflected Context.bindIsolatedService() method.
     */
    private static boolean bindServiceIsolated(@NonNull final Context context, @NonNull final String svcClassName, @NonNull final InstanceInfo info) {
        final Intent intent = new Intent();
        intent.setClassName(context, svcClassName);

        final String instanceId = info.getIdAsString();

        try {
            final Boolean result = (Boolean) sBindIsolatedService.invoke(context, intent, getAndroidFlags(info.getPriorityLevel()),
                                                                         instanceId, XPCOMEventTarget.launcherThread(), info);
            return result.booleanValue();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Wrapper that calls the reflected Context.bindService() overload that accepts an Executor argument.
     * We always specify the launcher thread as our Executor.
     */
    private static boolean bindServiceWithExecutor(@NonNull final Context context, @NonNull final Intent intent, @NonNull final InstanceInfo info) {
        try {
            final Boolean result = (Boolean) sBindServiceWithExecutor.invoke(context, intent, getAndroidFlags(info.getPriorityLevel()),
                                                                             XPCOMEventTarget.launcherThread(), info);
            return result.booleanValue();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    private static Method resolveBindIsolatedService() {
        try {
            return Context.class.getDeclaredMethod("bindIsolatedService", Intent.class, Integer.class, String.class, Executor.class, ServiceConnection.class);
        } catch (NoSuchMethodException e) {
            return null;
        } catch (Throwable e) {
            throw new RuntimeException(e);
        }
    }

    private static Method resolveBindServiceWithExecutor() {
        try {
            return Context.class.getDeclaredMethod("bindService", Intent.class, Integer.class, Executor.class, ServiceConnection.class);
        } catch (NoSuchMethodException e) {
            return null;
        } catch (Throwable e) {
            throw new RuntimeException(e);
        }
    }
}

