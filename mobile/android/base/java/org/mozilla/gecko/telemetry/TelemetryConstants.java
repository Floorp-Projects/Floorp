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

    public static final String ACTION_UPLOAD_CORE = "uploadCore";
    public static final String EXTRA_DEFAULT_SEARCH_ENGINE = "defaultSearchEngine";
    public static final String EXTRA_DOC_ID = "docId";
    public static final String EXTRA_PROFILE_NAME = "geckoProfileName";
    public static final String EXTRA_PROFILE_PATH = "geckoProfilePath";
    public static final String EXTRA_SEQ = "seq";

    public static final String PREF_SERVER_URL = "telemetry-serverUrl";
    public static final String PREF_SEQ_COUNT = "telemetry-seqCount";

    public static class CorePing {
        private CorePing() { /* To prevent instantiation */ }

        public static final String NAME = "core";
        public static final int VERSION_VALUE = 4; // For version history, see toolkit/components/telemetry/docs/core-ping.rst
        public static final String OS_VALUE = "Android";

        public static final String ARCHITECTURE = "arch";
        public static final String CLIENT_ID = "clientId";
        public static final String DEFAULT_SEARCH_ENGINE = "defaultSearch";
        public static final String DEVICE = "device";
        public static final String DISTRIBUTION_ID = "distributionId";
        public static final String EXPERIMENTS = "experiments";
        public static final String LOCALE = "locale";
        public static final String OS_ATTR = "os";
        public static final String OS_VERSION = "osversion";
        public static final String PROFILE_CREATION_DATE = "profileDate";
        public static final String SEQ = "seq";
        public static final String VERSION_ATTR = "v";
    }
}
