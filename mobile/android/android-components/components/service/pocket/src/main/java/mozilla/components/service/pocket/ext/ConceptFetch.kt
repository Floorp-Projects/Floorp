/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.ext

import androidx.annotation.WorkerThread
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.concept.fetch.isSuccess
import mozilla.components.service.pocket.logger
import java.io.IOException

// extension functions for :concept-fetch module.

/**
 * @return returns the string contained within the response body for the given [request] or null, on error.
 */
@WorkerThread // synchronous network call.
internal fun Client.fetchBodyOrNull(request: Request): String? {
    val response: Response? = try {
        fetch(request)
    } catch (e: IOException) {
        logger.debug("network error", e)
        null
    }

    return response?.use { if (response.isSuccess) response.body.string() else null }
}
