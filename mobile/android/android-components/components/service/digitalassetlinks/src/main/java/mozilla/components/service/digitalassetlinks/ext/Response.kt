/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.digitalassetlinks.ext

import mozilla.components.concept.fetch.Response
import org.json.JSONException

/**
 * Safely parse a JSON [Response] returned by an API.
 */
inline fun <T> Response.parseJsonBody(crossinline parser: (String) -> T): T? {
    val responseBody = use { body.string() }
    return try {
        parser(responseBody)
    } catch (e: JSONException) {
        null
    }
}
