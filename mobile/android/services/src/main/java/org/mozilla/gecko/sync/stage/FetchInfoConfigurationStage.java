/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.stage;

import android.os.SystemClock;

import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.HTTPFailureException;
import org.mozilla.gecko.sync.InfoConfiguration;
import org.mozilla.gecko.sync.JSONRecordFetcher;
import org.mozilla.gecko.sync.delegates.JSONRecordFetchDelegate;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.SyncStorageResponse;
import org.mozilla.gecko.sync.telemetry.TelemetryCollector;

/**
 * Fetches configuration data from info/configurations endpoint.
 */
public class FetchInfoConfigurationStage extends AbstractNonRepositorySyncStage {
    private final String configurationURL;
    private final AuthHeaderProvider authHeaderProvider;

    private static final String TELEMETRY_ERROR_NAME = "infoconfig";
    private static final String TELEMETRY_ERROR_MISSING = "missing";

    public FetchInfoConfigurationStage(final String configurationURL, final AuthHeaderProvider authHeaderProvider) {
        super();
        this.configurationURL = configurationURL;
        this.authHeaderProvider = authHeaderProvider;
    }

    public class StageInfoConfigurationDelegate implements JSONRecordFetchDelegate {
        @Override
        public void handleSuccess(final ExtendedJSONObject result) {
            session.config.infoConfiguration = new InfoConfiguration(result);
            telemetryStageCollector.finished = SystemClock.elapsedRealtime();
            session.advance();
        }

        @Override
        public void handleFailure(final SyncStorageResponse response) {
            telemetryStageCollector.finished = SystemClock.elapsedRealtime();

            // Handle all non-404 failures upstream.
            if (response.getStatusCode() != 404) {
                telemetryStageCollector.error = new TelemetryCollector.StageErrorBuilder()
                        .setLastException(new HTTPFailureException(response))
                        .build();
                session.handleHTTPError(response, "Failure fetching info/configuration");
                return;
            }

            // End-point might not be available (404) if server is running an older version.
            // We will use default config values in this case.
            // While this is not strictly an error in a sense that it's recoverable, going forward the
            // expectation of having info/config endpoint will solidify, and it should be easy enough
            // to interpret this error by correlating it to the current state of deployed servers.
            telemetryStageCollector.error = new TelemetryCollector
                    .StageErrorBuilder(TELEMETRY_ERROR_NAME, TELEMETRY_ERROR_MISSING)
                    .build();
            session.config.infoConfiguration = new InfoConfiguration();
            session.advance();
        }

        @Override
        public void handleError(final Exception e) {
            telemetryStageCollector.finished = SystemClock.elapsedRealtime();
            telemetryStageCollector.error = new TelemetryCollector.StageErrorBuilder()
                    .setLastException(e)
                    .build();
            session.abort(e, "Failure fetching info/configuration");
        }
    }
    @Override
    public void execute() {
        final StageInfoConfigurationDelegate delegate = new StageInfoConfigurationDelegate();
        final JSONRecordFetcher fetcher = new JSONRecordFetcher(configurationURL, authHeaderProvider);
        fetcher.fetch(delegate);
    }
}
