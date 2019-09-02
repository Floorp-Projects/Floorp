/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry;

/**
 * Holder of a static reference to the Telemetry instance. This is required for background services
 * that somehow need to get access to the configuration and storage. This is not particular nice.
 * Hopefully we can replace this with something better.
 *
 * @deprecated The whole service-telemetry library is deprecated. Please use the
 *              <a href="https://mozilla.github.io/glean/book/index.html">Glean SDK</a> instead.
 */
@Deprecated
public class TelemetryHolder {
    private static Telemetry telemetry;

    public static void set(Telemetry telemetry) {
        TelemetryHolder.telemetry = telemetry;
    }

    public static Telemetry get() {
        if (telemetry == null) {
            throw new IllegalStateException("You need to call set() on TelemetryHolder in your application class");
        }

        return telemetry;
    }
}
