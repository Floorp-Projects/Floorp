/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.telemetry;

import android.content.Context;
import android.support.annotation.WorkerThread;
import android.util.Log;
import org.mozilla.gecko.telemetry.pingbuilders.TelemetryCorePingBuilder;
import org.mozilla.gecko.telemetry.schedulers.TelemetryUploadScheduler;
import org.mozilla.gecko.telemetry.schedulers.TelemetryUploadAllPingsImmediatelyScheduler;
import org.mozilla.gecko.telemetry.stores.TelemetryJSONFilePingStore;
import org.mozilla.gecko.telemetry.stores.TelemetryPingStore;
import org.mozilla.gecko.util.ThreadUtils;

import java.io.File;
import java.io.IOException;

/**
 * The entry-point for Java-based telemetry. This class handles:
 *  * Initializing the Stores & Schedulers.
 *  * Queueing upload requests for a given ping.
 *
 * To test Telemetry , see {@link TelemetryConstants} &
 * https://wiki.mozilla.org/Mobile/Fennec/Android/Java_telemetry.
 *
 * The full architecture is:
 *
 * Fennec -(PingBuilder)-> Dispatcher -2-> Scheduler -> UploadService
 *                             | 1                            |
 *                           Store <--------------------------
 *
 * The store acts as a single store of truth and contains a list of all
 * pings waiting to be uploaded. The dispatcher will queue a ping to upload
 * by writing it to the store. Later, the UploadService will try to upload
 * this queued ping by reading directly from the store.
 *
 * To implement a new ping type, you should:
 *   1) Implement a {@link org.mozilla.gecko.telemetry.pingbuilders.TelemetryPingBuilder} for your ping type.
 *   2) Re-use a ping store in .../stores/ or implement a new one: {@link TelemetryPingStore}. The
 * type of store may be affected by robustness requirements (e.g. do you have data in addition to
 * pings that need to be atomically updated when a ping is stored?) and performance requirements.
 *   3) Re-use an upload scheduler in .../schedulers/ or implement a new one: {@link TelemetryUploadScheduler}.
 *   4) Initialize your Store & (if new) Scheduler in the constructor of this class
 *   5) Add a queuePingForUpload method for your PingBuilder class (see
 * {@link #queuePingForUpload(Context, TelemetryCorePingBuilder)})
 *   6) In Fennec, where you want to store a ping and attempt upload, create a PingBuilder and
 * pass it to the new queuePingForUpload method.
 */
public class TelemetryDispatcher {
    private static final String LOGTAG = "Gecko" + TelemetryDispatcher.class.getSimpleName();

    private static final String STORE_CONTAINER_DIR_NAME = "telemetry_java";
    private static final String CORE_STORE_DIR_NAME = "core";

    private final TelemetryJSONFilePingStore coreStore;

    private final TelemetryUploadAllPingsImmediatelyScheduler uploadAllPingsImmediatelyScheduler;

    @WorkerThread // via TelemetryJSONFilePingStore
    public TelemetryDispatcher(final String profilePath, final String profileName) {
        final String storePath = profilePath + File.separator + STORE_CONTAINER_DIR_NAME;

        // There are measurements in the core ping (e.g. seq #) that would ideally be atomically updated
        // when the ping is stored. However, for simplicity, we use the json store and accept the possible
        // loss of data (see bug 1243585 comment 16+ for more).
        coreStore = new TelemetryJSONFilePingStore(new File(storePath, CORE_STORE_DIR_NAME), profileName);

        uploadAllPingsImmediatelyScheduler = new TelemetryUploadAllPingsImmediatelyScheduler();
    }

    private void queuePingForUpload(final Context context, final TelemetryPing ping, final TelemetryPingStore store,
            final TelemetryUploadScheduler scheduler) {
        final QueuePingRunnable runnable = new QueuePingRunnable(context, ping, store, scheduler);
        ThreadUtils.postToBackgroundThread(runnable); // TODO: Investigate how busy this thread is. See if we want another.
    }

    /**
     * Queues the given ping for upload and potentially schedules upload. This method can be called from any thread.
     */
    public void queuePingForUpload(final Context context, final TelemetryCorePingBuilder pingBuilder) {
        final TelemetryPing ping = pingBuilder.build();
        queuePingForUpload(context, ping, coreStore, uploadAllPingsImmediatelyScheduler);
    }

    private static class QueuePingRunnable implements Runnable {
        private final Context applicationContext;
        private final TelemetryPing ping;
        private final TelemetryPingStore store;
        private final TelemetryUploadScheduler scheduler;

        public QueuePingRunnable(final Context context, final TelemetryPing ping, final TelemetryPingStore store,
                final TelemetryUploadScheduler scheduler) {
            this.applicationContext = context.getApplicationContext();
            this.ping = ping;
            this.store = store;
            this.scheduler = scheduler;
        }

        @Override
        public void run() {
            // We block while storing the ping so the scheduled upload is guaranteed to have the newly-stored value.
            try {
                store.storePing(ping);
            } catch (final IOException e) {
                // Don't log exception to avoid leaking profile path.
                Log.e(LOGTAG, "Unable to write ping to disk. Continuing with upload attempt");
            }

            if (scheduler.isReadyToUpload(store)) {
                scheduler.scheduleUpload(applicationContext, store);
            }
        }
    }
}
