/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import org.json.JSONArray;

import java.util.List;

public class ExperimentsMeasurement extends TelemetryMeasurement {
    private static final String FIELD_NAME = "experiments";
    private JSONArray experimentsIds;

    public ExperimentsMeasurement() {
        super(FIELD_NAME);
        experimentsIds = new JSONArray();
    }

    public void setActiveExperiments(List<String> activeExperimentsIds) {
        experimentsIds = new JSONArray(activeExperimentsIds);
    }

    @Override
    public Object flush() {
        return experimentsIds;
    }
}
