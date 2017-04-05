/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.schedule.jobscheduler;

import android.app.job.JobParameters;
import android.app.job.JobService;
import android.os.AsyncTask;
import android.support.annotation.VisibleForTesting;
import android.util.Log;

import org.mozilla.telemetry.Telemetry;
import org.mozilla.telemetry.TelemetryHolder;
import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.mozilla.telemetry.net.TelemetryClient;
import org.mozilla.telemetry.ping.TelemetryPingBuilder;
import org.mozilla.telemetry.storage.TelemetryStorage;

public class TelemetryJobService extends JobService {
    private static final String LOG_TAG = "TelemetryJobService";

    private UploadPingsTask uploadTask;

    @Override
    public boolean onStartJob(JobParameters params) {
        uploadTask = new UploadPingsTask();
        uploadTask.execute(params);
        return true;
    }

    @Override
    public boolean onStopJob(JobParameters params) {
        if (uploadTask != null) {
            uploadTask.cancel(true);
        }
        return true;
    }

    private class UploadPingsTask extends AsyncTask<JobParameters, Void, Void> {
        @Override
        protected Void doInBackground(JobParameters... params) {
            final JobParameters parameters = params[0];
            uploadPingsInBackground(this, parameters);
            return null;
        }
    }

    @VisibleForTesting public void uploadPingsInBackground(AsyncTask task, JobParameters parameters) {
        final Telemetry telemetry = TelemetryHolder.get();
        final TelemetryStorage storage = telemetry.getStorage();

        for (TelemetryPingBuilder builder : telemetry.getBuilders()) {
            final String pingType = builder.getType();
            Log.d(LOG_TAG, "Performing upload of ping type: " + pingType);

            if (task.isCancelled()) {
                Log.d(LOG_TAG, "Job stopped. Exiting.");
                return; // Job will be rescheduled from onStopJob().
            }

            if (storage.countStoredPings(pingType) == 0) {
                Log.d(LOG_TAG, "No pings of type " + pingType + " to upload");
                continue;
            }

            if (!performPingUpload(telemetry, pingType)) {
                Log.i(LOG_TAG, "Upload failed. Rescheduling job.");
                jobFinished(parameters, true);
                return;
            }
        }

        Log.d(LOG_TAG, "All uploads performed");
        jobFinished(parameters, false);
    }

    private boolean performPingUpload(Telemetry telemetry, String pingType) {
        final TelemetryConfiguration configuration = telemetry.getConfiguration();
        final TelemetryStorage storage = telemetry.getStorage();
        final TelemetryClient client = telemetry.getClient();

        return storage.process(pingType, new TelemetryStorage.TelemetryStorageCallback() {
            @Override
            public boolean onTelemetryPingLoaded(String path, String serializedPing) {
                return client.uploadPing(configuration, path, serializedPing);
            }
        });

    }
}
