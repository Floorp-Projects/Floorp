/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.telemetry.pingbuilders;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.telemetry.TelemetryOutgoingPing;

import java.util.Set;
import java.util.UUID;

/**
 * A generic Builder for {@link TelemetryOutgoingPing} instances. Each overriding class is
 * expected to create a specific type of ping (e.g. "core").
 *
 * This base class handles the common ping operations under the hood:
 *   * Validating mandatory fields
 *   * Forming the server url
 */
abstract class TelemetryPingBuilder {
    // In the server url, the initial path directly after the "scheme://host:port/"
    private static final String SERVER_INITIAL_PATH = "submit/telemetry";

    // By default Fennec ping's use the old telemetry version, this can be overridden
    private static final int DEFAULT_TELEMETRY_VERSION = 1;

    // Unified telemetry is version 4
    public static final int UNIFIED_TELEMETRY_VERSION = 4;

    // We deliberately call the OS/platform Android to avoid confusion with desktop Linux
    public static final String OS_NAME = "Android";

    private final String serverPath;
    protected final ExtendedJSONObject payload;
    protected final String docID;

    public TelemetryPingBuilder() {
        this(DEFAULT_TELEMETRY_VERSION);
    }

    public TelemetryPingBuilder(int version) {
        docID = UUID.randomUUID().toString();
        serverPath = getTelemetryServerPath(getDocType(), docID, version);
        payload = new ExtendedJSONObject();
    }

    /**
     * @return the name of the ping (e.g. "core")
     */
    public abstract String getDocType();

    /**
     * @return the fields that are mandatory for the resultant ping to be uploaded to
     *         the server. These will be validated before the ping is built.
     */
    public abstract String[] getMandatoryFields();

    public TelemetryOutgoingPing build() {
        validatePayload();
        return new TelemetryOutgoingPing(serverPath, payload, docID);
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
     * @param docType The name of the ping (e.g. "main")
     * @param docID A UUID that identifies the ping
     * @param version The ping format version
     * @return a url at which to POST the telemetry data to
     */
    private static String getTelemetryServerPath(final String docType, final String docID, int version) {
        final String appName = AppConstants.MOZ_APP_BASENAME;
        final String appVersion = AppConstants.MOZ_APP_VERSION;
        final String appUpdateChannel = AppConstants.MOZ_UPDATE_CHANNEL;
        final String appBuildId = AppConstants.MOZ_APP_BUILDID;

        // The compiler will optimize a single String concatenation into a StringBuilder statement.
        // If you change this `return`, be sure to keep it as a single statement to keep it optimized!
        return SERVER_INITIAL_PATH + '/' +
                docID + '/' +
                docType + '/' +
                appName + '/' +
                appVersion + '/' +
                appUpdateChannel + '/' +
                appBuildId +
                (version == UNIFIED_TELEMETRY_VERSION ? "?v=4" : "");
    }
}
