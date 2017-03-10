/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.storage;

import android.support.annotation.RestrictTo;
import android.util.Log;

import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.mozilla.telemetry.ping.TelemetryPing;
import org.mozilla.telemetry.serialize.TelemetryPingSerializer;
import org.mozilla.telemetry.util.FileUtils;
import org.mozilla.telemetry.util.IOUtils;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.FilenameFilter;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.util.regex.Pattern;

/**
 * TelemetryStorage implementation that stores pings as files on disk.
 */
public class FileTelemetryStorage implements TelemetryStorage {
    private static final String LOG_TAG = "FileTelemetryStorage";

    private static final String FILE_PATTERN = "[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}";
    private static final String STORAGE_DIRECTORY = "storage";

    private final TelemetryConfiguration configuration;
    private final TelemetryPingSerializer serializer;

    private final File storageDirectory;

    public FileTelemetryStorage(TelemetryConfiguration configuration, TelemetryPingSerializer serializer) {
        this.configuration = configuration;
        this.serializer = serializer;

        this.storageDirectory = new File(configuration.getDataDirectory(), STORAGE_DIRECTORY);

        FileUtils.assertDirectory(storageDirectory);
    }

    @Override
    public void store(TelemetryPing ping) {
        final File pingStorageDirectory = new File(storageDirectory, ping.getType());
        FileUtils.assertDirectory(pingStorageDirectory);

        final String serializedPing = serializer.serialize(ping);

        final File pingFile = new File(pingStorageDirectory, ping.getDocumentId());

        FileOutputStream stream = null;

        try {
            stream = new FileOutputStream(pingFile, true);

            final BufferedWriter writer = new BufferedWriter(new OutputStreamWriter(stream));
            writer.write(ping.getUploadPath());
            writer.newLine();
            writer.write(serializedPing);
            writer.newLine();
            writer.flush();
            writer.close();
        } catch (IOException e) {
            Log.w(LOG_TAG, "IOException while writing event to disk", e);
        } finally {
            IOUtils.safeClose(stream);
        }
    }

    @Override
    public boolean process(String pingType, TelemetryStorageCallback callback) {
        final File pingStorageDirectory = new File(storageDirectory, pingType);

        Pattern uuidPattern = Pattern.compile(FILE_PATTERN);

        final FilenameFilter uuidFilenameFilter = new FileUtils.FilenameRegexFilter(uuidPattern);
        final File[] files = pingStorageDirectory.listFiles(uuidFilenameFilter);
        if (files == null) {
            return true;
        }

        for (File file : files) {
            FileReader reader = null;

            try {
                final BufferedReader bufferedReader = new BufferedReader(reader = new FileReader(file));
                final String path = bufferedReader.readLine();
                final String serializedPing = bufferedReader.readLine();

                boolean processed = callback.onTelemetryPingLoaded(path, serializedPing);

                if (processed) {
                    if (!file.delete()) {
                        Log.w(LOG_TAG, "Could not delete local ping file after processing");
                    }
                } else {
                    // The callback couldn't process this file. Let's stop and rety later.
                    return false;
                }
            } catch (FileNotFoundException e) {
                // This shouldn't happen after we queried the directory. But whatever. Let's continue.
            } catch(IOException e) {
                // Something is not right. Let's stop.
                return false;
            } finally {
                IOUtils.safeClose(reader);
            }
        }

        return true;
    }

    @Override
    @RestrictTo(RestrictTo.Scope.LIBRARY)
    public int countStoredPings(String pingType) {
        final File pingStorageDirectory = new File(storageDirectory, pingType);

        Pattern uuidPattern = Pattern.compile(FILE_PATTERN);

        final FilenameFilter uuidFilenameFilter = new FileUtils.FilenameRegexFilter(uuidPattern);
        final File[] files = pingStorageDirectory.listFiles(uuidFilenameFilter);

        return files != null ? files.length : 0;
    }
}
