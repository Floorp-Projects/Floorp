/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import android.os.Handler
import android.os.Looper
import androidx.test.platform.app.InstrumentationRegistry
import okhttp3.mockwebserver.Dispatcher
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import okhttp3.mockwebserver.RecordedRequest
import okio.Buffer
import okio.source
import java.io.IOException
import java.io.InputStream

/**
 * A [MockWebServer] [Dispatcher] that will return a generic search results page in the body of
 * requests and responds with status 200.
 *
 * If the dispatcher is unable to read a requested asset, it will fail the test by throwing an
 * Exception on the main thread.
 *
 * @sample [org.mozilla.fenix.ui.SearchTest]
 */
class SearchDispatcher : Dispatcher() {
    private val mainThreadHandler = Handler(Looper.getMainLooper())

    override fun dispatch(request: RecordedRequest): MockResponse {
        val assetManager = InstrumentationRegistry.getInstrumentation().context.assets
        try {
            // When we perform a search with the custom search engine, returns the generic4.html test page as search results
            if (request.path!!.contains("searchResults.html?search=")) {
                MockResponse().setResponseCode(HTTP_OK)
                val path = "pages/generic4.html"
                assetManager.open(path).use { inputStream ->
                    return fileToResponse(inputStream)
                }
            }
            return MockResponse().setResponseCode(HTTP_NOT_FOUND)
        } catch (e: IOException) {
            // e.g. file not found.
            // We're on a background thread so we need to forward the exception to the main thread.
            mainThreadHandler.postAtFrontOfQueue { throw e }
            return MockResponse().setResponseCode(HTTP_NOT_FOUND)
        }
    }
}

@Throws(IOException::class)
private fun fileToResponse(file: InputStream): MockResponse {
    return MockResponse()
        .setResponseCode(HTTP_OK)
        .setBody(fileToBytes(file))
}

@Throws(IOException::class)
private fun fileToBytes(file: InputStream): Buffer {
    val result = Buffer()
    result.writeAll(file.source())
    return result
}
