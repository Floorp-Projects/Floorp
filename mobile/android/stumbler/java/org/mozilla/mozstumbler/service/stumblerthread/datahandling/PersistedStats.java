/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.mozstumbler.service.stumblerthread.datahandling;

import org.mozilla.mozstumbler.service.AppGlobals;
import org.mozilla.mozstumbler.service.utils.TelemetryWrapper;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Properties;
import java.util.concurrent.TimeUnit;

class PersistedStats {
    private final File mStatsFile;

    public PersistedStats(String baseDir) {
        mStatsFile = new File(baseDir, "upload_stats.ini");
    }

    public synchronized Properties readSyncStats() throws IOException {
        if (!mStatsFile.exists()) {
            return new Properties();
        }

        final FileInputStream input = new FileInputStream(mStatsFile);
        try {
            final Properties props = new Properties();
            props.load(input);
            return props;
        } finally {
            input.close();
        }
    }

    public synchronized void incrementSyncStats(long bytesSent, long reports, long cells, long wifis) throws IOException {
        if (reports + cells + wifis < 1) {
            return;
        }

        final Properties properties = readSyncStats();
        final long time = System.currentTimeMillis();
        final long lastUploadTime = Long.parseLong(properties.getProperty(DataStorageContract.Stats.KEY_LAST_UPLOAD_TIME, "0"));
        final long storedObsPerDay = Long.parseLong(properties.getProperty(DataStorageContract.Stats.KEY_OBSERVATIONS_PER_DAY, "0"));
        long observationsToday = reports;
        if (lastUploadTime > 0) {
            long dayLastUploaded = TimeUnit.MILLISECONDS.toDays(lastUploadTime);
            long dayDiff = TimeUnit.MILLISECONDS.toDays(time) - dayLastUploaded;
            if (dayDiff > 0) {
                // send value of store obs per day
                TelemetryWrapper.addToHistogram(AppGlobals.TELEMETRY_OBSERVATIONS_PER_DAY,
                        Long.valueOf(storedObsPerDay / dayDiff).intValue());
            } else {
                observationsToday += storedObsPerDay;
            }
        }

        writeSyncStats(time,
            Long.parseLong(properties.getProperty(DataStorageContract.Stats.KEY_BYTES_SENT, "0")) + bytesSent,
            Long.parseLong(properties.getProperty(DataStorageContract.Stats.KEY_OBSERVATIONS_SENT, "0")) + reports,
            Long.parseLong(properties.getProperty(DataStorageContract.Stats.KEY_CELLS_SENT, "0")) + cells,
            Long.parseLong(properties.getProperty(DataStorageContract.Stats.KEY_WIFIS_SENT, "0")) + wifis,
            observationsToday);


        final long lastUploadMs = Long.parseLong(properties.getProperty(DataStorageContract.Stats.KEY_LAST_UPLOAD_TIME, "0"));
        final int timeDiffSec = Long.valueOf((time - lastUploadMs) / 1000).intValue();
        if (lastUploadMs > 0 && timeDiffSec > 0) {
            TelemetryWrapper.addToHistogram(AppGlobals.TELEMETRY_TIME_BETWEEN_UPLOADS_SEC, timeDiffSec);
            TelemetryWrapper.addToHistogram(AppGlobals.TELEMETRY_BYTES_UPLOADED_PER_SEC, Long.valueOf(bytesSent).intValue() / timeDiffSec);
        }
        TelemetryWrapper.addToHistogram(AppGlobals.TELEMETRY_BYTES_PER_UPLOAD, Long.valueOf(bytesSent).intValue());
        TelemetryWrapper.addToHistogram(AppGlobals.TELEMETRY_OBSERVATIONS_PER_UPLOAD, Long.valueOf(reports).intValue());
        TelemetryWrapper.addToHistogram(AppGlobals.TELEMETRY_WIFIS_PER_UPLOAD, Long.valueOf(wifis).intValue());
        TelemetryWrapper.addToHistogram(AppGlobals.TELEMETRY_CELLS_PER_UPLOAD, Long.valueOf(cells).intValue());
    }

    public synchronized void writeSyncStats(long time, long bytesSent, long totalObs,
                                            long totalCells, long totalWifis, long obsPerDay) throws IOException {
        final FileOutputStream out = new FileOutputStream(mStatsFile);
        try {
            final Properties props = new Properties();
            props.setProperty(DataStorageContract.Stats.KEY_LAST_UPLOAD_TIME, String.valueOf(time));
            props.setProperty(DataStorageContract.Stats.KEY_BYTES_SENT, String.valueOf(bytesSent));
            props.setProperty(DataStorageContract.Stats.KEY_OBSERVATIONS_SENT, String.valueOf(totalObs));
            props.setProperty(DataStorageContract.Stats.KEY_CELLS_SENT, String.valueOf(totalCells));
            props.setProperty(DataStorageContract.Stats.KEY_WIFIS_SENT, String.valueOf(totalWifis));
            props.setProperty(DataStorageContract.Stats.KEY_VERSION, String.valueOf(DataStorageContract.Stats.VERSION_CODE));
            props.setProperty(DataStorageContract.Stats.KEY_OBSERVATIONS_PER_DAY, String.valueOf(obsPerDay));
            props.store(out, null);
        } finally {
            out.close();
        }
    }

}
