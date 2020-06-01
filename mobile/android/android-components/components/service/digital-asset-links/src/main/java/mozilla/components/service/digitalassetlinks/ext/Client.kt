/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.digitalassetlinks.ext

import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.concept.fetch.Response.Companion.SUCCESS
import java.io.IOException

internal fun Client.safeFetch(request: Request): Response? {
    return try {
        val response = fetch(request)
        if (response.status == SUCCESS) response else null
    } catch (e: IOException) {
        null
    }
}
