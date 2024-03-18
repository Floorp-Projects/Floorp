/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.helpers

import android.os.Handler
import android.os.Looper
import androidx.core.net.toUri
import androidx.test.platform.app.InstrumentationRegistry
import okhttp3.mockwebserver.Dispatcher
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import okhttp3.mockwebserver.RecordedRequest
import okio.Buffer
import okio.source
import java.io.IOException
import java.io.InputStream

object MockWebServerHelper {
    /**
     * A [MockWebServer] [Dispatcher] that will return Android assets in the body of requests.
     *
     * If the dispatcher is unable to read a requested asset, it will fail the test by throwing an
     * Exception on the main thread.
     *
     */
    const val HTTP_OK = 200
    const val HTTP_NOT_FOUND = 404

    class AndroidAssetDispatcher : Dispatcher() {
        private val mainThreadHandler = Handler(Looper.getMainLooper())

        override fun dispatch(request: RecordedRequest): MockResponse {
            val assetManager = InstrumentationRegistry.getInstrumentation().context.assets
            try {
                val pathWithoutQueryParams = request.path!!.drop(1).toUri().path
                assetManager.open(pathWithoutQueryParams!!).use { inputStream ->
                    return fileToResponse(pathWithoutQueryParams, inputStream)
                }
            } catch (e: IOException) { // e.g. file not found.
                // We're on a background thread so we need to forward the exception to the main thread.
                mainThreadHandler.postAtFrontOfQueue { throw e }
                return MockResponse().setResponseCode(HTTP_NOT_FOUND)
            }
        }
    }

    @Throws(IOException::class)
    private fun fileToResponse(path: String, file: InputStream): MockResponse {
        return MockResponse()
            .setResponseCode(HTTP_OK)
            .setBody(fileToBytes(file)!!)
            .addHeader("content-type: " + contentType(path))
    }

    @Throws(IOException::class)
    private fun fileToBytes(file: InputStream): Buffer? {
        val result = Buffer()
        result.writeAll(file.source())
        return result
    }

    private fun contentType(path: String): String? {
        return when {
            path.endsWith(".png") -> "image/png"
            path.endsWith(".jpg") -> "image/jpeg"
            path.endsWith(".jpeg") -> "image/jpeg"
            path.endsWith(".gif") -> "image/gif"
            path.endsWith(".svg") -> "image/svg+xml"
            path.endsWith(".html") -> "text/html; charset=utf-8"
            path.endsWith(".txt") -> "text/plain; charset=utf-8"
            else -> "application/octet-stream"
        }
    }
}
