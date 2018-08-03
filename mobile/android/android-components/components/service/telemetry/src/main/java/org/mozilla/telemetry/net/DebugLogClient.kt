/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.net

import mozilla.components.support.base.log.logger.Logger
import org.json.JSONException
import org.json.JSONObject
import org.mozilla.telemetry.config.TelemetryConfiguration

/**
 * This client just prints pings to logcat instead of uploading them. Therefore this client is only
 * useful for debugging purposes.
 */
class DebugLogClient(private val tag: String) : TelemetryClient {
    private val logger = Logger("telemetry/debug")

    override fun uploadPing(configuration: TelemetryConfiguration, path: String, serializedPing: String): Boolean {
        logger.debug("---PING--- $path")

        try {
            val json = JSONObject(serializedPing)
            logger.debug(json.toString(2))
        } catch (e: JSONException) {
            logger.debug("Corrupt JSON", e)
        }

        return true
    }
}
