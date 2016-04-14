/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.telemetry.pings;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.sync.ExtendedJSONObject;

import java.util.Set;
import java.util.UUID;

/**
 * A generic Builder for {@link TelemetryPing} instances. Each overriding class is
 * expected to create a specific type of ping (e.g. "core").
 *
 * This base class handles the common ping operations under the hood:
 *   * Validating mandatory fields
 *   * Forming the server url
 */
abstract class TelemetryPingBuilder {
    // In the server url, the initial path directly after the "scheme://host:port/"
    private static final String SERVER_INITIAL_PATH = "submit/telemetry";

    private final String serverUrl;
    protected final ExtendedJSONObject payload;

    public TelemetryPingBuilder(final String serverURLSchemeHostPort) {
        serverUrl = getTelemetryServerURL(getDocType(), serverURLSchemeHostPort);
        payload = new ExtendedJSONObject();
    }

    /**
     * @return the name of the ping (e.g. "core")
     */
    abstract String getDocType();

    /**
     * @return the fields that are mandatory for the resultant ping to be uploaded to
     *         the server. These will be validated before the ping is built.
     */
    abstract String[] getMandatoryFields();

    public TelemetryPing build() {
        validatePayload();
        return new TelemetryPing(serverUrl, payload);
    }

    private void validatePayload() {
        final Set<String> keySet = payload.keySet();
        for (final String mandatoryField : getMandatoryFields()) {
            if (!keySet.contains(mandatoryField)) {
                throw new IllegalArgumentException("Builder does not contain mandatory field: " +
                        mandatoryField);
            }
        }
    }

    /**
     * Returns a url of the format:
     *   http://hostname/submit/telemetry/docId/docType/appName/appVersion/appUpdateChannel/appBuildID
     *
     * @param serverURLSchemeHostPort The server url with the scheme, host, and port (e.g. "http://mozilla.org:80")
     * @param docType The name of the ping (e.g. "main")
     * @return a url at which to POST the telemetry data to
     */
    private static String getTelemetryServerURL(final String docType,
            final String serverURLSchemeHostPort) {
        final String docId = UUID.randomUUID().toString();
        final String appName = AppConstants.MOZ_APP_BASENAME;
        final String appVersion = AppConstants.MOZ_APP_VERSION;
        final String appUpdateChannel = AppConstants.MOZ_UPDATE_CHANNEL;
        final String appBuildId = AppConstants.MOZ_APP_BUILDID;

        // The compiler will optimize a single String concatenation into a StringBuilder statement.
        // If you change this `return`, be sure to keep it as a single statement to keep it optimized!
        return serverURLSchemeHostPort + '/' +
                SERVER_INITIAL_PATH + '/' +
                docId + '/' +
                docType + '/' +
                appName + '/' +
                appVersion + '/' +
                appUpdateChannel + '/' +
                appBuildId;
    }
}
