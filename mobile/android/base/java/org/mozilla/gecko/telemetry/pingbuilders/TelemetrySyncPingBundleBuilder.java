/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.telemetry.pingbuilders;

import android.os.Build;
import android.support.annotation.NonNull;

import org.json.simple.JSONArray;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.telemetry.TelemetryOutgoingPing;
import org.mozilla.gecko.telemetry.TelemetryPing;
import org.mozilla.gecko.telemetry.stores.TelemetryPingStore;

import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.TimeZone;

/**
 * Responsible for building a Sync Ping, based on the telemetry docs:
 * http://gecko.readthedocs.io/en/latest/toolkit/components/telemetry/telemetry/data/sync-ping.html
 *
 * This builder takes two stores ('sync' and 'event') and produces a single "sync ping".
 *
 * Note that until Bug 1363924, event telemetry will be ignored.
 *
 * Sample result will look something like:
 * {
 *     "syncs": [list of syncs, as produced by the SyncBuilder],
 *     "events": [list of events, as produced by the EventBuilder]
 * }
 */
public class TelemetrySyncPingBundleBuilder extends TelemetryPingBuilder {
    private static final String PING_TYPE = "sync";
    private static final int PING_BUNDLE_VERSION = 5; // Bug 1374758
    private static final int PING_SYNC_DATA_FORMAT_VERSION = 1; // Bug 1374758

    public static final String UPLOAD_REASON_FIRST = "first";
    public static final String UPLOAD_REASON_CLOCK_DRIFT = "clockdrift";
    public static final String UPLOAD_REASON_SCHEDULE = "schedule";
    public static final String UPLOAD_REASON_IDCHANGE = "idchange";
    public static final String UPLOAD_REASON_COUNT = "count";

    private final ExtendedJSONObject pingData = new ExtendedJSONObject();

    @Override
    public String getDocType() {
        return "sync";
    }

    @Override
    public String[] getMandatoryFields() {
        return new String[0];
    }

    public TelemetrySyncPingBundleBuilder setReason(@NonNull String reason) {
        pingData.put("why", reason);
        return this;
    }

    @Override
    public TelemetryOutgoingPing build() {
        final DateFormat pingCreationDateFormat = new SimpleDateFormat(
                "yyyy-MM-dd'T'HH:mm:ss'Z'", Locale.US);
        pingCreationDateFormat.setTimeZone(TimeZone.getTimeZone("UTC"));

        payload.put("type", PING_TYPE);
        payload.put("version", PING_BUNDLE_VERSION);
        payload.put("id", docID);
        payload.put("creationDate", pingCreationDateFormat.format(new Date()));

        final ExtendedJSONObject application = new ExtendedJSONObject();
        application.put("architecture", Build.CPU_ABI);
        application.put("buildId", AppConstants.MOZ_APP_BUILDID);
        application.put("platformVersion", AppConstants.MOZ_APP_VERSION);
        application.put("name", AppConstants.MOZ_APP_BASENAME);
        application.put("version", AppConstants.MOZ_APP_VERSION);
        application.put("displayVersion", AppConstants.MOZ_APP_VERSION);
        application.put("vendor", AppConstants.MOZ_APP_VENDOR);
        application.put("xpcomAbi", AppConstants.MOZ_APP_ABI);
        application.put("channel", AppConstants.MOZ_UPDATE_CHANNEL);

        payload.put("application", application);

        pingData.put("version", PING_SYNC_DATA_FORMAT_VERSION);

        payload.put("payload", pingData);
        return super.build();
    }

    @SuppressWarnings("unchecked")
    public TelemetrySyncPingBundleBuilder setSyncStore(TelemetryPingStore store) {
        final JSONArray syncs = new JSONArray();
        List<TelemetryPing> pings = store.getAllPings();

        // Please note how we're not including constituent ping's docID in the final payload. This is
        // unfortunate and causes some grief when managing local ping storage and uploads, but needs
        // to be resolved beyond this individual client. See Bug 1369186.
        for (TelemetryPing ping : pings) {
            syncs.add(ping.getPayload());
        }

        pingData.put("syncs", syncs);
        return this;
    }

    // Event telemetry will be implemented in Bug 1363924.
    public TelemetrySyncPingBundleBuilder setSyncEventStore(TelemetryPingStore store) {
        return this;
    }
}
