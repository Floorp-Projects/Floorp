/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import android.content.SharedPreferences;
import androidx.annotation.VisibleForTesting;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONException;
import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.mozilla.telemetry.event.TelemetryEvent;
import org.mozilla.telemetry.util.IOUtils;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;

import mozilla.components.support.base.log.logger.Logger;

public class EventsMeasurement extends TelemetryMeasurement {
    private static final String LOG_TAG = EventsMeasurement.class.getSimpleName();

    private static final int VERSION = 1;
    private static final String FIELD_NAME = "events";

    private static final String PREFERENCE_EVENT_COUNT = "event_count";

    private TelemetryConfiguration configuration;
    private Logger logger;
    private String filename;

    public EventsMeasurement(TelemetryConfiguration configuration) {
        this(configuration, "events");
    }

    public EventsMeasurement(TelemetryConfiguration configuration, String filename) {
        super(FIELD_NAME);

        this.configuration = configuration;
        this.logger = new Logger("telemetry/events");
        this.filename = filename;
    }

    public EventsMeasurement add(final TelemetryEvent event) {
        saveEventToDisk(event);
        return this;
    }

    @Override
    public Object flush() {
        return readAndClearEventsFromDisk();
    }

    private synchronized JSONArray readAndClearEventsFromDisk() {
        final JSONArray events = new JSONArray();
        final File file = getEventFile();

        FileInputStream stream = null;

        try {
            stream = new FileInputStream(file);

            final BufferedReader reader = new BufferedReader(new InputStreamReader(stream));

            String line;

            while ((line = reader.readLine()) != null) {
                try {
                    JSONArray event = new JSONArray(line);
                    events.put(event);

                    resetEventCount();
                } catch (JSONException e) {
                    // Let's log a warning and move on. This event is lost.
                    logger.warn("Could not parse event from disk", e);
                }
            }
        } catch (FileNotFoundException e) {
            // This shouldn't happen because we do not create event pings if there are no events.
            // However in case the file disappears: Continue with no events.
            return new JSONArray();
        } catch (IOException e) {
            // Handling this exception at this time is tricky: We might have been able to read some
            // events at the time this exception occurred. We can either try to add them to the
            // ping and remove the file or we retry later again.

            // We just log an error here. This means we are going to continue building the ping
            // with the events we were able to read from disk. The events file will be removed and
            // we might potentially lose events that we couldn't ready because of the exception.
            logger.warn("IOException while reading events from disk", e);
        } finally {
            IOUtils.safeClose(stream);

            if (!file.delete()) {
                logger.warn("Events file could not be deleted", new IOException());
            }
        }

        return events;
    }

    @VisibleForTesting File getEventFile() {
        return new File(configuration.getDataDirectory(), filename + VERSION);
    }

    private synchronized void saveEventToDisk(TelemetryEvent event) {
        FileOutputStream stream = null;

        try {
            stream = new FileOutputStream(getEventFile(), true);

            final BufferedWriter writer = new BufferedWriter(new OutputStreamWriter(stream));
            writer.write(event.toJSON());
            writer.newLine();
            writer.flush();
            writer.close();

            countEvent();
        } catch (IOException e) {
            logger.warn("IOException while writing event to disk", e);
        } finally {
            IOUtils.safeClose(stream);
        }
    }

    private synchronized void countEvent() {
        final SharedPreferences preferences = configuration.getSharedPreferences();

        long count = preferences.getLong(PREFERENCE_EVENT_COUNT, 0);

        preferences.edit()
                .putLong(PREFERENCE_EVENT_COUNT, ++count)
                .apply();
    }

    private synchronized void resetEventCount() {
        final SharedPreferences preferences = configuration.getSharedPreferences();

        preferences.edit()
                .putLong(PREFERENCE_EVENT_COUNT, 0)
                .apply();
    }

    public long getEventCount() {
        final SharedPreferences preferences = configuration.getSharedPreferences();

        return preferences.getLong(PREFERENCE_EVENT_COUNT, 0);
    }
}
