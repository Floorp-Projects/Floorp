/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard.source.kinto

import mozilla.components.service.fretboard.ExperimentDownloadException
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL

/**
 * HttpURLConnection-based Http client
 */
internal class HttpURLConnectionHttpClient : HttpClient {
    override fun get(url: URL, headers: Map<String, String>?): String {
        var urlConnection: HttpURLConnection? = null
        try {
            urlConnection = url.openConnection() as HttpURLConnection
            urlConnection.requestMethod = "GET"
            urlConnection.useCaches = false
            headers?.forEach { urlConnection.setRequestProperty(it.key, it.value) }

            val responseCode = urlConnection.responseCode
            if (responseCode !in 200..299)
                throw ExperimentDownloadException("Status code: $responseCode")

            return urlConnection.inputStream.bufferedReader().use { it.readText() }
        } catch (e: IOException) {
            throw ExperimentDownloadException(e.message)
        } finally {
            urlConnection?.disconnect()
        }
    }
}