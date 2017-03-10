/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.event;

import android.os.SystemClock;
import android.support.annotation.CheckResult;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.RestrictTo;

import org.json.JSONArray;
import org.json.JSONObject;
import org.mozilla.telemetry.TelemetryHolder;
import org.mozilla.telemetry.ping.TelemetryEventPingBuilder;

import java.util.Map;

/**
 * TelemetryEvent specifies a common events data format, which allows for broader, shared usage of
 * data processing tools.
 */
public class TelemetryEvent {
    private static final long startTime = SystemClock.elapsedRealtime();

    /**
     * Create a new event with mandatory category, method and object.
     *
     * @param category identifier. The category is a group name for events and helps to avoid name conflicts.
     * @param method identifier. This describes the type of event that occured, e.g. click, keydown or focus.
     * @param object identifier. This is the object the event occured on, e.g. reload_button or urlbar.
     */
    @CheckResult
    public static TelemetryEvent create(@NonNull String category, @NonNull String method, @Nullable String object) {
        final TelemetryEvent event = new TelemetryEvent();

        event.category = category;
        event.method = method;
        event.object = object;

        return event;
    }

    /**
     * Create a new event with mandatory category, method, object, value and extras.
     *
     * @param category identifier. The category is a group name for events and helps to avoid name conflicts.
     * @param method identifier. This describes the type of event that occured, e.g. click, keydown or focus.
     * @param object identifier. This is the object the event occured on, e.g. reload_button or urlbar.
     * @param value This is a user defined value, providing context for the event.
     * @param extras This is an map of the form {"key": "value", ...}, used for events where additional context is needed.
     */
    @CheckResult
    public static TelemetryEvent create(@NonNull String category, @NonNull String method, @Nullable String object, String value, Map<String, Object> extras) {
        final TelemetryEvent event = new TelemetryEvent();

        event.category = category;
        event.method = method;
        event.object = object;
        event.value = value;
        event.extras = extras;

        return event;
    }

    /**
     * Positive number. This is the time in ms when the event was recorded, relative to the main
     * process start time.
     */
    private long timestamp;

    /**
     * Identifier. The category is a group name for events and helps to avoid name conflicts.
     */
    private String category;

    /**
     * Identifier. This describes the type of event that occurred, e.g. click, keydown or focus.
     */
    private String method;

    /**
     * Identifier. This is the object the event occurred on, e.g. reload_button or urlbar.
     */
    private String object;

    /**
     * Optional, may be null. This is a user defined value, providing context for the event.
     */
    private @Nullable  String value;

    /**
     * Optional, may be null. This is an object of the form {"key": "value", ...}, used for events
     * where additional context is needed.
     */
    private @Nullable  Map<String, Object> extras;

    private TelemetryEvent() {
        timestamp = SystemClock.elapsedRealtime() - startTime;
    }

    /**
     * Queue this event to be sent with the next event ping.
     */
    public Thread queue() {
        final TelemetryEventPingBuilder builder = (TelemetryEventPingBuilder) TelemetryHolder.get()
                .getBuilder(TelemetryEventPingBuilder.TYPE);

        return builder.getEventsMeasurement()
                .add(this);
    }

    /**
     * Create a JSON representation of this event for storing and sending it.
     */
    @RestrictTo(RestrictTo.Scope.LIBRARY)
    public String toJSON() {
        final JSONArray array = new JSONArray();

        array.put(timestamp);
        array.put(category);
        array.put(method);
        array.put(object);

        if (value != null) {
            array.put(value);
        }

        if (extras != null) {
            if (value == null) {
                array.put(null);
            }

            array.put(new JSONObject(extras));
        }

        return array.toString();
    }
}
