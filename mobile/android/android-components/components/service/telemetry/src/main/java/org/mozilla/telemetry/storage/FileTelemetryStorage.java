/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.storage;

import androidx.annotation.RestrictTo;
import androidx.annotation.VisibleForTesting;
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
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.regex.Pattern;

import mozilla.components.support.base.log.logger.Logger;

/**
 * TelemetryStorage implementation that stores pings as files on disk.
 */
public class FileTelemetryStorage implements TelemetryStorage {
    private static final String FILE_PATTERN = "[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}";
    private static final String STORAGE_DIRECTORY = "storage";

    private final Logger logger;
    private final TelemetryConfiguration configuration;
    private final TelemetryPingSerializer serializer;

    private final File storageDirectory;

    public FileTelemetryStorage(TelemetryConfiguration configuration, TelemetryPingSerializer serializer) {
        this.logger = new Logger("telemetry/storage");
        this.configuration = configuration;
        this.serializer = serializer;

        this.storageDirectory = new File(configuration.getDataDirectory(), STORAGE_DIRECTORY);

        FileUtils.assertDirectory(storageDirectory);
    }

    @Override
    public synchronized void store(TelemetryPing ping) {
        storePing(ping);
        maybePrunePings(ping.getType());
    }

    @Override
    public boolean process(String pingType, TelemetryStorageCallback callback) {
        for (File file : listPingFiles(pingType)) {
            FileReader reader = null;

            try {
                final BufferedReader bufferedReader = new BufferedReader(reader = new FileReader(file));
                final String path = bufferedReader.readLine();
                final String serializedPing = bufferedReader.readLine();

                final boolean processed = serializedPing == null || callback.onTelemetryPingLoaded(path, serializedPing);

                if (processed) {
                    if (!file.delete()) {
                        logger.warn("Could not delete local ping file after processing", new IOException());
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

    private void storePing(TelemetryPing ping) {
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
            logger.warn("IOException while writing event to disk", e);
        } finally {
            IOUtils.safeClose(stream);
        }
    }

    private void maybePrunePings(final String pingType) {
        final File[] files = listPingFiles(pingType);

        final int pingsToRemove = files.length - configuration.getMaximumNumberOfPingsPerType();

        if (pingsToRemove <= 0) {
            return;
        }

        final List<File> sortedFiles = new ArrayList<>(Arrays.asList(files));
        Collections.sort(sortedFiles, new FileUtils.FileLastModifiedComparator());

        for (File file : sortedFiles) {
            System.out.println(file.lastModified() + " " + file.getAbsolutePath());
        }

        for (int i = 0; i < pingsToRemove; i++) {
            final File file = sortedFiles.get(i);

            if (!file.delete()) {
                logger.warn("Can't prune ping file: " + file.getAbsolutePath(), new IOException());
            }
        }
    }

    @VisibleForTesting File[] listPingFiles(String pingType) {
        final File pingStorageDirectory = new File(storageDirectory, pingType);

        final Pattern uuidPattern = Pattern.compile(FILE_PATTERN);

        final FilenameFilter uuidFilenameFilter = new FileUtils.FilenameRegexFilter(uuidPattern);
        final File[] files = pingStorageDirectory.listFiles(uuidFilenameFilter);
        if (files == null) {
            return new File[0];
        }
        return files;
    }

    @Override
    @RestrictTo(RestrictTo.Scope.LIBRARY)
    public int countStoredPings(String pingType) {
        return listPingFiles(pingType).length;
    }
}
