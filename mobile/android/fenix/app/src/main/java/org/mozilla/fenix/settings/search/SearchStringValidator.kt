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
import java.net.HttpURLConnection

object SearchStringValidator {
    enum class Result { Success, CannotReach }

    private const val QUERY_PARAM = "1"

    fun isSearchStringValid(client: Client, searchString: String): Result {
        val request = createRequest(searchString)
        val response = try {
            client.fetch(request)
        } catch (e: IOException) {
            return Result.CannotReach
        } catch (e: IllegalArgumentException) {
            return Result.CannotReach
        }

        // read the response stream to ensure the body is closed correctly. workaround for https://bugzilla.mozilla.org/show_bug.cgi?id=1603114
        response.close()
        return if (response.isSuccess ||
            isTestQueryParamNotFound(response.status)
        ) {
            Result.Success
        } else {
            Result.CannotReach
        }
    }

    private fun createRequest(searchString: String): Request {
        // we should share the code to substitute and normalize the search string (see SearchEngine.buildSearchUrl).
        val encodedTestQuery = Uri.encode(QUERY_PARAM)

        val normalizedHttpsSearchUrlStr = searchString.toNormalizedUrl()
        val searchUrl = normalizedHttpsSearchUrlStr.replace("%s".toRegex(), encodedTestQuery)
        return Request(searchUrl)
    }

    /**
     * There is no universal query param, so a site returning 404 doesn't mean the user input is wrong,
     * it means that the website's search functionality doesn't find our test param, but it's reachable.
     */
    private fun isTestQueryParamNotFound(status: Int): Boolean =
        status == HttpURLConnection.HTTP_NOT_FOUND
}
