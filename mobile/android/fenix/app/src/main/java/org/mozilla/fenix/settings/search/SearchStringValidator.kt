/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.search

import android.net.Uri
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.isSuccess
import mozilla.components.support.ktx.kotlin.toNormalizedUrl
import java.io.IOException

object SearchStringValidator {
    enum class Result { Success, CannotReach }

    fun isSearchStringValid(client: Client, searchString: String): Result {
        val request = createRequest(searchString)
        val response = try {
            client.fetch(request)
        } catch (e: IOException) {
            return Result.CannotReach
        }
        return if (response.isSuccess) Result.Success else Result.CannotReach
    }

    private fun createRequest(searchString: String): Request {
        // we should share the code to substitute and normalize the search string (see SearchEngine.buildSearchUrl).
        val encodedTestQuery = Uri.encode("testSearchEngineValidation")

        val normalizedHttpsSearchUrlStr = searchString.toNormalizedUrl()
        val searchUrl = normalizedHttpsSearchUrlStr.replace("%s".toRegex(), encodedTestQuery)
        return Request(searchUrl)
    }
}
