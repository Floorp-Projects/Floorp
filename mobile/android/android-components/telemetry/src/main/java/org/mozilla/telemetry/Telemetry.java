/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry;

import android.support.annotation.RestrictTo;

import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.mozilla.telemetry.net.TelemetryClient;
import org.mozilla.telemetry.ping.TelemetryCorePingBuilder;
import org.mozilla.telemetry.ping.TelemetryPing;
import org.mozilla.telemetry.ping.TelemetryPingBuilder;
import org.mozilla.telemetry.schedule.TelemetryScheduler;
import org.mozilla.telemetry.storage.TelemetryStorage;

import java.util.HashMap;
import java.util.Map;

public class Telemetry {
    private final TelemetryConfiguration configuration;
    private final TelemetryStorage storage;
    private final TelemetryClient client;
    private final TelemetryScheduler scheduler;

    private final Map<String, TelemetryPingBuilder> pingBuilders;

    public Telemetry(TelemetryConfiguration configuration, TelemetryStorage storage,
                     TelemetryClient client, TelemetryScheduler scheduler) {
        this.configuration = configuration;
        this.storage = storage;
        this.client = client;
        this.scheduler = scheduler;

        pingBuilders = new HashMap<>();
    }

    public Telemetry addPingBuilder(TelemetryPingBuilder builder) {
        pingBuilders.put(builder.getType(), builder);
        return this;
    }

    // TODO: Do queuing asynchronously? (Issue #16)
    public Telemetry queuePing(String pingType) {
        if (!configuration.isCollectionEnabled()) {
            return this;
        }

        final TelemetryPingBuilder pingBuilder = pingBuilders.get(pingType);

        if (!pingBuilder.canBuild()) {
            // We do not always want to build a ping. Sometimes we want to collect enough data so that
            // it is worth sending a ping. Here we exit early if the ping builder implementation
            // signals that it's not time to build a ping yet.
            return this;
        }

        final TelemetryPing ping = pingBuilder.build();
        storage.store(ping);

        return this;
    }

    public TelemetryPingBuilder getBuilder(String pingType) {
        return pingBuilders.get(pingType);
    }

    public Telemetry scheduleUpload(String pingType) {
        if (!configuration.isUploadEnabled()) {
            return this;
        }

        if (storage.countStoredPings(pingType) == 0) {
            // There are no pings to upload.
            return this;
        }

        scheduler.scheduleUpload(configuration, pingType);
        return this;
    }

    public void recordSessionStart() {
        if (!configuration.isCollectionEnabled()) {
            return;
        }

        if (!pingBuilders.containsKey(TelemetryCorePingBuilder.TYPE)) {
            throw new IllegalStateException("This configuration does not contain a core ping builder");
        }

        final TelemetryCorePingBuilder builder = (TelemetryCorePingBuilder) pingBuilders.get(TelemetryCorePingBuilder.TYPE);


        builder.getSessionDurationMeasurement().recordSessionStart();
        builder.getSessionCountMeasurement().countSession();
    }

    public Telemetry recordSessionEnd() {
        if (!configuration.isCollectionEnabled()) {
            return this;
        }

        if (!pingBuilders.containsKey(TelemetryCorePingBuilder.TYPE)) {
            throw new IllegalStateException("This configuration does not contain a core ping builder");
        }

        final TelemetryCorePingBuilder builder = (TelemetryCorePingBuilder) pingBuilders.get(TelemetryCorePingBuilder.TYPE);
        builder.getSessionDurationMeasurement().recordSessionEnd();

        return this;
    }

    @RestrictTo(RestrictTo.Scope.LIBRARY)
    public TelemetryClient getClient() {
        return client;
    }

    @RestrictTo(RestrictTo.Scope.LIBRARY)
    public TelemetryStorage getStorage() {
        return storage;
    }

    @RestrictTo(RestrictTo.Scope.LIBRARY)
    public TelemetryConfiguration getConfiguration() {
        return configuration;
    }
}
