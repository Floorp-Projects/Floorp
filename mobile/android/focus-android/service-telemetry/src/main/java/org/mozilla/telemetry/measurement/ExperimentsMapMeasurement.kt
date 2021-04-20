/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement

import org.json.JSONObject

private const val FIELD_NAME = "experiments"

private const val VALUE_CONTROL = "control"
private const val VALUE_BRANCH = "branch"

class ExperimentsMapMeasurement : TelemetryMeasurement(FIELD_NAME) {
    private val map = JSONObject()

    fun setExperiments(experiments: Map<String, Boolean>) {
        experiments.forEach { entry ->
            map.put(entry.key, if (entry.value) VALUE_BRANCH else VALUE_CONTROL)
        }
    }

    override fun flush() = map
}
