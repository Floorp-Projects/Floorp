/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry;

import android.support.annotation.NonNull;
import android.support.annotation.RestrictTo;
import android.support.annotation.VisibleForTesting;

import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.mozilla.telemetry.event.TelemetryEvent;
import org.mozilla.telemetry.measurement.DefaultSearchMeasurement;
import org.mozilla.telemetry.measurement.EventsMeasurement;
import org.mozilla.telemetry.net.TelemetryClient;
import org.mozilla.telemetry.ping.TelemetryCorePingBuilder;
import org.mozilla.telemetry.ping.TelemetryEventPingBuilder;
import org.mozilla.telemetry.ping.TelemetryMobileEventPingBuilder;
import org.mozilla.telemetry.ping.TelemetryPing;
import org.mozilla.telemetry.ping.TelemetryPingBuilder;
import org.mozilla.telemetry.schedule.TelemetryScheduler;
import org.mozilla.telemetry.storage.TelemetryStorage;

import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class Telemetry {
    private final TelemetryConfiguration configuration;
    private final TelemetryStorage storage;
    private final TelemetryClient client;
    private final TelemetryScheduler scheduler;

    private final Map<String, TelemetryPingBuilder> pingBuilders;
    private final ExecutorService executor = Executors.newSingleThreadExecutor();

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

    public Telemetry queuePing(final String pingType) {
        if (!configuration.isCollectionEnabled()) {
            return this;
        }

        executor.submit(new Runnable() {
            @Override
            public void run() {
                final TelemetryPingBuilder pingBuilder = pingBuilders.get(pingType);

                if (!pingBuilder.canBuild()) {
                    // We do not always want to build a ping. Sometimes we want to collect enough data so that
                    // it is worth sending a ping. Here we exit early if the ping builder implementation
                    // signals that it's not time to build a ping yet.
                    return;
                }

                final TelemetryPing ping = pingBuilder.build();
                storage.store(ping);
            }
        });

        return this;
    }

    public Telemetry queueEvent(final TelemetryEvent event) {
        if (!configuration.isCollectionEnabled()) {
            return this;
        }

        executor.submit(new Runnable() {
            @Override
            public void run() {
                // We migrated from focus-event to mobile-event and unfortunately, this code was hard-coded to expect
                // a focus-event ping builder. We work around this by checking our new hardcoded code first for the new
                // ping type and then falling back on the legacy ping type.
                final TelemetryPingBuilder mobileEventBuilder = pingBuilders.get(TelemetryMobileEventPingBuilder.TYPE);
                final TelemetryPingBuilder focusEventBuilder = pingBuilders.get(TelemetryEventPingBuilder.TYPE);
                final EventsMeasurement measurement;
                final String addedPingType;
                if (mobileEventBuilder != null) {
                    measurement = ((TelemetryMobileEventPingBuilder) mobileEventBuilder).getEventsMeasurement();
                    addedPingType = mobileEventBuilder.getType();
                } else if (focusEventBuilder != null) {
                    measurement = ((TelemetryEventPingBuilder) focusEventBuilder).getEventsMeasurement();
                    addedPingType = focusEventBuilder.getType();
                } else {
                    throw new IllegalStateException("Expect either TelemetryEventPingBuilder or " +
                            "TelemetryMobileEventPingBuilder to be added to queue events");
                }

                measurement.add(event);
                if (measurement.getEventCount() >= configuration.getMaximumNumberOfEventsPerPing()) {
                    queuePing(addedPingType);
                }
            }
        });

        return this;
    }

    public Collection<TelemetryPingBuilder> getBuilders() {
        return pingBuilders.values();
    }

    public Telemetry scheduleUpload() {
        if (!configuration.isUploadEnabled()) {
            return this;
        }

        executor.submit(new Runnable() {
            @Override
            public void run() {
                scheduler.scheduleUpload(configuration);
            }
        });
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

    /**
     * Record a search for the given location and search engine identifier.
     *
     * Common location values used by Fennec and Focus:
     *
     * actionbar:  the user types in the url bar and hits enter to use the default search engine
     * listitem:   the user selects a search engine from the list of secondary search engines at
     *             the bottom of the screen
     * suggestion: the user clicks on a search suggestion or, in the case that suggestions are
     *             disabled, the row corresponding with the main engine
     *
     * @param location where search was started.
     * @param identifier of the used search engine.
     */
    public Telemetry recordSearch(@NonNull  String location, @NonNull String identifier) {
        if (!configuration.isCollectionEnabled()) {
            return this;
        }

        if (!pingBuilders.containsKey(TelemetryCorePingBuilder.TYPE)) {
            throw new IllegalStateException("This configuration does not contain a core ping builder");
        }

        final TelemetryCorePingBuilder builder = (TelemetryCorePingBuilder) pingBuilders.get(TelemetryCorePingBuilder.TYPE);
        builder.getSearchesMeasurement()
                .recordSearch(location, identifier);

        return this;
    }

    /**
     * Records the list of active experiments
     *
     * @param activeExperimentsIds list of active experiments ids
     */
    public Telemetry recordActiveExperiments(List<String> activeExperimentsIds) {
        if (!configuration.isCollectionEnabled()) {
            return this;
        }

        if (!pingBuilders.containsKey(TelemetryCorePingBuilder.TYPE)) {
            throw new IllegalStateException("This configuration does not contain a core ping builder");
        }

        final TelemetryCorePingBuilder builder = (TelemetryCorePingBuilder) pingBuilders.get(TelemetryCorePingBuilder.TYPE);
        builder.getExperimentsMeasurement()
                .setActiveExperiments(activeExperimentsIds);

        return this;
    }

    public Telemetry setDefaultSearchProvider(DefaultSearchMeasurement.DefaultSearchEngineProvider provider) {
        if (!pingBuilders.containsKey(TelemetryCorePingBuilder.TYPE)) {
            throw new IllegalStateException("This configuration does not contain a core ping builder");
        }

        final TelemetryCorePingBuilder builder = (TelemetryCorePingBuilder) pingBuilders.get(TelemetryCorePingBuilder.TYPE);
        builder.getDefaultSearchMeasurement()
                .setDefaultSearchEngineProvider(provider);

        return this;
    }

    public TelemetryClient getClient() {
        return client;
    }

    public TelemetryStorage getStorage() {
        return storage;
    }

    public TelemetryConfiguration getConfiguration() {
        return configuration;
    }

    /**
     * @hide
     */
    @RestrictTo(RestrictTo.Scope.LIBRARY)
    @VisibleForTesting ExecutorService getExecutor() {
        return executor;
    }
}
