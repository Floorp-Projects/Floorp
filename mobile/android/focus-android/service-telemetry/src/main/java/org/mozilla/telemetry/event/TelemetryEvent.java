/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.event;

import android.os.SystemClock;
import androidx.annotation.CheckResult;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RestrictTo;
import org.json.JSONArray;
import org.json.JSONObject;
import org.mozilla.telemetry.TelemetryHolder;
import org.mozilla.telemetry.util.StringUtils;

import java.util.HashMap;
import java.util.Map;

/**
 * TelemetryEvent specifies a common events data format, which allows for broader, shared usage of
 * data processing tools.
 */
public class TelemetryEvent {
    private static final long startTime = SystemClock.elapsedRealtime();

    private static final int MAX_LENGTH_CATEGORY = 30;
    private static final int MAX_LENGTH_METHOD = 20;
    private static final int MAX_LENGTH_OBJECT = 20;
    private static final int MAX_LENGTH_VALUE = 80;
    private static final int MAX_EXTRA_KEYS = 200;
    private static final int MAX_LENGTH_EXTRA_KEY = 15;
    private static final int MAX_LENGTH_EXTRA_VALUE = 80;

    /**
     * Create a new event with mandatory category, method and object.
     *
     * @param category identifier. The category is a group name for events and helps to avoid name conflicts.
     * @param method identifier. This describes the type of event that occured, e.g. click, keydown or focus.
     * @param object identifier. This is the object the event occured on, e.g. reload_button or urlbar.
     */
    @CheckResult
    public static TelemetryEvent create(@NonNull String category, @NonNull String method, @Nullable String object) {
        return new TelemetryEvent(category, method, object, null);
    }

    /**
     * Create a new event with mandatory category, method, object and value.
     *
     * @param category identifier. The category is a group name for events and helps to avoid name conflicts.
     * @param method identifier. This describes the type of event that occured, e.g. click, keydown or focus.
     * @param object identifier. This is the object the event occured on, e.g. reload_button or urlbar.
     * @param value This is a user defined value, providing context for the event.
     */
    @CheckResult
    public static TelemetryEvent create(@NonNull String category, @NonNull String method, @Nullable String object, String value) {
        return new TelemetryEvent(category, method, object, value);
    }

    /**
     * Positive number. This is the time in ms when the event was recorded, relative to the main
     * process start time.
     */
    private final long timestamp;

    /**
     * Identifier. The category is a group name for events and helps to avoid name conflicts.
     */
    private final String category;

    /**
     * Identifier. This describes the type of event that occurred, e.g. click, keydown or focus.
     */
    private final String method;

    /**
     * Identifier. This is the object the event occurred on, e.g. reload_button or urlbar.
     */
    private @Nullable final String object;

    /**
     * Optional, may be null. This is a user defined value, providing context for the event.
     */
    private @Nullable String value;

    /**
     * Optional, may be null. This is an object of the form {"key": "value", ...}, used for events
     * where additional context is needed.
     */
    private final Map<String, Object> extras;

    private TelemetryEvent(@NonNull String category, @NonNull String method, @Nullable String object, @Nullable String value) {
        timestamp = SystemClock.elapsedRealtime() - startTime;

        // We are naively assuming that all strings here are ASCII (1 character = 1 bytes) as the max lengths are defined in bytes
        // There's an opportunity here to make this more strict and throw - however we may want to make this configurable.
        this.category = StringUtils.safeSubstring(category, 0, MAX_LENGTH_CATEGORY);
        this.method = StringUtils.safeSubstring(method, 0, MAX_LENGTH_METHOD);
        this.object = object == null ? null : StringUtils.safeSubstring(object, 0, MAX_LENGTH_OBJECT);
        this.value = value == null ? null : StringUtils.safeSubstring(value, 0, MAX_LENGTH_VALUE);
        this.extras = new HashMap<>();
    }

    public TelemetryEvent extra(String key, String value) {
        if (extras.size() > MAX_EXTRA_KEYS) {
            throw new IllegalArgumentException("Exceeding limit of " + MAX_EXTRA_KEYS + " extra keys");
        }

        extras.put(StringUtils.safeSubstring(key, 0, MAX_LENGTH_EXTRA_KEY),
                StringUtils.safeSubstring(value, 0, MAX_LENGTH_EXTRA_VALUE));

        return this;
    }

    /**
     * Queue this event to be sent with the next event ping.
     */
    public void queue() {
        TelemetryHolder.get().queueEvent(this);
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

        if (extras != null && !extras.isEmpty()) {
            if (value == null) {
                array.put(null);
            }

            array.put(new JSONObject(extras));
        }

        return array.toString();
    }
}
