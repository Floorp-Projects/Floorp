/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.telemetry;

public class TelemetryConstants {

    public static class CorePing {
        private CorePing() { /* To prevent instantiation */ }

        public static final String NAME = "core";
        public static final int VERSION_VALUE = 1;
        public static final String OS_VALUE = "Android";

        public static final String ARCHITECTURE = "arch";
        public static final String CLIENT_ID = "clientId";
        public static final String DEVICE = "device";
        public static final String LOCALE = "locale";
        public static final String OS_ATTR = "os";
        public static final String OS_VERSION = "osversion";
        public static final String SEQ = "seq";
        public static final String VERSION_ATTR = "v";
    }
}
