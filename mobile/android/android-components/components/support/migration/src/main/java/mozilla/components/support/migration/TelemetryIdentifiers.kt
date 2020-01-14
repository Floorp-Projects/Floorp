/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import mozilla.components.lib.crash.CrashReporter
import mozilla.components.support.base.log.logger.Logger
import org.json.JSONException
import org.json.JSONObject
import java.io.File
import java.io.IOException

/**
 * Result of a telemetry identifier migration.
 */
sealed class TelemetryIdentifiersResult {
    /**
     * Success results.
     */
    sealed class Success : TelemetryIdentifiersResult() {
        /**
         * Present identifiers obtained.
         */
        data class Identifiers(val clientId: String?, val profileCreationDate: Long?) : Success()
    }
}

internal object TelemetryIdentifiersMigration {
    private val logger = Logger("TelemetryIdentifiersMigration")

    // Same constants as defined in GeckoProfile.
    // See https://searchfox.org/mozilla-esr68/source/mobile/android/geckoview/src/main/java/org/mozilla/gecko/GeckoProfile.java#44-53
    private const val CLIENT_ID_FILE_PATH = "datareporting/state.json"
    private const val CLIENT_ID_JSON_ATTR = "clientID"

    private const val TIMES_PATH = "times.json"
    private const val PROFILE_CREATION_DATE_JSON_ATTR = "created"

    @SuppressWarnings("TooGenericExceptionCaught")
    internal fun migrate(profilePath: String, crashReporter: CrashReporter): Result<TelemetryIdentifiersResult> {
        val clientStateFile = File(profilePath, CLIENT_ID_FILE_PATH)

        val clientId = try {
            parseClientId(clientStateFile.readText())
        } catch (e: Exception) {
            logger.error("Error getting clientId", e)
            crashReporter.submitCaughtException(e)
            null
        }

        val creationDate = try {
            parseCreationDate(File(profilePath, TIMES_PATH).readText())
        } catch (e: Exception) {
            logger.error("Error getting creation date", e)
            crashReporter.submitCaughtException(e)
            null
        }

        return Result.Success(
            TelemetryIdentifiersResult.Success.Identifiers(
                clientId = clientId, profileCreationDate = creationDate
            )
        )
    }

    @Throws(IOException::class, JSONException::class)
    private fun parseClientId(json: String): String {
        val obj = JSONObject(json)
        return obj.getString(CLIENT_ID_JSON_ATTR)
    }

    @Throws(IOException::class, JSONException::class)
    private fun parseCreationDate(json: String): Long {
        val obj = JSONObject(json)
        return obj.getLong(PROFILE_CREATION_DATE_JSON_ATTR)
    }
}
