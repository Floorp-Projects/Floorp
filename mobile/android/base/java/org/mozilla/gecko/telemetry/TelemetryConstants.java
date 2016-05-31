/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.telemetry;

import org.mozilla.gecko.AppConstants;

public class TelemetryConstants {

    // Change these two values to enable upload in developer builds.
    public static final boolean UPLOAD_ENABLED = AppConstants.MOZILLA_OFFICIAL; // Disabled for developer builds.
    public static final String DEFAULT_SERVER_URL = "https://incoming.telemetry.mozilla.org";

    public static final String USER_AGENT =
            "Firefox-Android-Telemetry/" + AppConstants.MOZ_APP_VERSION + " (" + AppConstants.MOZ_APP_UA_NAME + ")";

    public static final String PREF_SERVER_URL = "telemetry-serverUrl";
}
