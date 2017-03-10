/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.schedule.jobscheduler;

import android.app.job.JobParameters;
import android.app.job.JobService;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.support.annotation.VisibleForTesting;
import android.text.TextUtils;

import org.mozilla.telemetry.Telemetry;
import org.mozilla.telemetry.TelemetryHolder;
import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.mozilla.telemetry.net.TelemetryClient;
import org.mozilla.telemetry.storage.TelemetryStorage;

public class TelemetryJobService extends JobService {
    public static final String EXTRA_PING_TYPE = "ping_type";

    private volatile Looper serviceLooper;
    private volatile ServiceHandler serviceHandler;

    private final class ServiceHandler extends Handler {
        private ServiceHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            performUpload((JobParameters) msg.obj);
            stopSelf(msg.arg1);
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();

        final HandlerThread thread = new HandlerThread("TelemetryService");
        thread.start();

        serviceLooper = thread.getLooper();
        serviceHandler = new ServiceHandler(serviceLooper);
    }

    @Override
    public void onDestroy() {
        serviceLooper.quit();
    }

    @Override
    public boolean onStartJob(JobParameters params) {
        final String pingType = params.getExtras().getString(EXTRA_PING_TYPE);
        if (TextUtils.isEmpty(pingType)) {
            return false;
        }

        final Message msg = serviceHandler.obtainMessage();
        msg.obj = params;
        serviceHandler.sendMessage(msg);

        return true;
    }

    @Override
    public boolean onStopJob(JobParameters params) {
        return false;
    }

    @VisibleForTesting
    public void performUpload(JobParameters params) {
        final Telemetry telemetry = TelemetryHolder.get();
        final TelemetryConfiguration configuration = telemetry.getConfiguration();
        final TelemetryClient client = telemetry.getClient();

        final String pingType = params.getExtras().getString(EXTRA_PING_TYPE);

        final TelemetryStorage storage = telemetry.getStorage();
        final boolean processed = storage.process(pingType, new TelemetryStorage.TelemetryStorageCallback() {
            @Override
            public boolean onTelemetryPingLoaded(String path, String serializedPing) {
                return client.uploadPing(configuration, path, serializedPing);
            }
        });

        jobFinished(params, !processed);
    }
}
