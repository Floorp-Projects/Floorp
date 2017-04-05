/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import android.util.Log;

import org.mozilla.gecko.background.common.log.Logger;

/**
 * Wraps and provides access to configuration data returned from info/configuration.
 * Docs: https://docs.services.mozilla.com/storage/apis-1.5.html#general-info
 *
 * - <bold>max_request_bytes</bold>: the maximum size in bytes of the overall
 *   HTTP request body that will be accepted by the server.
 *
 * - <bold>max_post_records</bold>: the maximum number of records that can be
 *   uploaded to a collection in a single POST request.
 *
 * - <bold>max_post_bytes</bold>: the maximum combined size in bytes of the
 *   record payloads that can be uploaded to a collection in a single
 *   POST request.
 *
 * - <bold>max_total_records</bold>: the maximum number of records that can be
 *   uploaded to a collection as part of a batched upload.
 *
 * - <bold>max_total_bytes</bold>: the maximum combined size in bytes of the
 *   record payloads that can be uploaded to a collection as part of
 *   a batched upload.
 *
 * - <bold>max_record_payload_bytes</bold>: the maximum size of an individual
 *   BSO payload, in bytes.
 */
public class InfoConfiguration {
    private static final String LOG_TAG = "InfoConfiguration";

    public static final String MAX_REQUEST_BYTES = "max_request_bytes";
    public static final String MAX_POST_RECORDS = "max_post_records";
    public static final String MAX_POST_BYTES = "max_post_bytes";
    public static final String MAX_TOTAL_RECORDS = "max_total_records";
    public static final String MAX_TOTAL_BYTES = "max_total_bytes";
    public static final String MAX_PAYLOAD_BYTES = "max_record_payload_bytes";

    private static final long DEFAULT_MAX_REQUEST_BYTES = 1048576;
    private static final long DEFAULT_MAX_POST_RECORDS = 100;
    private static final long DEFAULT_MAX_POST_BYTES = 1048576;
    private static final long DEFAULT_MAX_TOTAL_RECORDS = 10000;
    private static final long DEFAULT_MAX_TOTAL_BYTES = 104857600;
    private static final long DEFAULT_MAX_PAYLOAD_BYTES = 262144;

    // While int's upper range is (2^31-1), which in bytes is equivalent to 2.147 GB, let's be optimistic
    // about the future and use long here, so that this code works if the server decides its clients are
    // all on fiber and have congress-library sized bookmark collections.
    // Record counts are long for the sake of simplicity.
    public final long maxRequestBytes;
    public final long maxPostRecords;
    public final long maxPostBytes;
    public final long maxTotalRecords;
    public final long maxTotalBytes;
    public final long maxPayloadBytes;

    public InfoConfiguration() {
        Logger.debug(LOG_TAG, "info/configuration is unavailable, using defaults");

        maxRequestBytes = DEFAULT_MAX_REQUEST_BYTES;
        maxPostRecords = DEFAULT_MAX_POST_RECORDS;
        maxPostBytes = DEFAULT_MAX_POST_BYTES;
        maxTotalRecords = DEFAULT_MAX_TOTAL_RECORDS;
        maxTotalBytes = DEFAULT_MAX_TOTAL_BYTES;
        maxPayloadBytes = DEFAULT_MAX_PAYLOAD_BYTES;
    }

    public InfoConfiguration(final ExtendedJSONObject record) {
        Logger.debug(LOG_TAG, "info/configuration is " + record.toJSONString());

        maxRequestBytes = getValueFromRecord(record, MAX_REQUEST_BYTES, DEFAULT_MAX_REQUEST_BYTES);
        maxPostRecords = getValueFromRecord(record, MAX_POST_RECORDS, DEFAULT_MAX_POST_RECORDS);
        maxPostBytes = getValueFromRecord(record, MAX_POST_BYTES, DEFAULT_MAX_POST_BYTES);
        maxTotalRecords = getValueFromRecord(record, MAX_TOTAL_RECORDS, DEFAULT_MAX_TOTAL_RECORDS);
        maxTotalBytes = getValueFromRecord(record, MAX_TOTAL_BYTES, DEFAULT_MAX_TOTAL_BYTES);
        maxPayloadBytes = getValueFromRecord(record, MAX_PAYLOAD_BYTES, DEFAULT_MAX_PAYLOAD_BYTES);
    }

    private static Long getValueFromRecord(ExtendedJSONObject record, String key, long defaultValue) {
        if (!record.containsKey(key)) {
            return defaultValue;
        }

        try {
            Long val = record.getLong(key);
            if (val == null) {
                return defaultValue;
            }
            return val;
        } catch (NumberFormatException e) {
            Log.w(LOG_TAG, "Could not parse key " + key + " from record: " + record, e);
            return defaultValue;
        }
    }
}
