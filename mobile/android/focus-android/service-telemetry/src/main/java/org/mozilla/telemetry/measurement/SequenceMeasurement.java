/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import android.content.SharedPreferences;

import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.mozilla.telemetry.ping.TelemetryPingBuilder;

public class SequenceMeasurement extends TelemetryMeasurement {
    private static final String FIELD_NAME = "seq";

    private static final String PREFERENCE_PREFIX = "sequence_";

    private final TelemetryConfiguration configuration;
    private final String preferenceKeySequence;

    public SequenceMeasurement(TelemetryConfiguration configuration, TelemetryPingBuilder ping) {
        super(FIELD_NAME);

        this.configuration = configuration;
        this.preferenceKeySequence = PREFERENCE_PREFIX + ping.getType();
    }

    @Override
    public Object flush() {
        return getAndIncrementSequence();
    }

    private synchronized long getAndIncrementSequence() {
        final SharedPreferences preferences = configuration.getSharedPreferences();

        long sequence = preferences.getLong(preferenceKeySequence, 0);

        preferences.edit()
                .putLong(preferenceKeySequence, ++sequence)
                .apply();

        return sequence;
    }
}
