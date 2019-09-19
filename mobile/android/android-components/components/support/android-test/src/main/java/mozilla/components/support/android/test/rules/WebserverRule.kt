/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.android.test.rules

import android.os.Handler
import android.os.Looper
import androidx.test.platform.app.InstrumentationRegistry
import okhttp3.mockwebserver.Dispatcher
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import okhttp3.mockwebserver.RecordedRequest
import org.junit.rules.TestWatcher
import org.junit.runner.Description
import java.io.IOException

/**
 * A [TestWatcher] junit rule that will serve content from assets in the test package.
 */
class WebserverRule : TestWatcher() {
    private val webserver: MockWebServer = MockWebServer().apply {
        setDispatcher(AndroidAssetDispatcher())
    }

    fun url(path: String = ""): String {
        return webserver.url(path).toString()
    }

    override fun starting(description: Description?) {
        webserver.start()
    }

    override fun finished(description: Description?) {
        webserver.shutdown()
    }
}

private const val HTTP_OK = 200
private const val HTTP_NOT_FOUND = 404

private class AndroidAssetDispatcher : Dispatcher() {
    private val mainThreadHandler = Handler(Looper.getMainLooper())

    override fun dispatch(request: RecordedRequest): MockResponse {
        var path = request.path.drop(1)
        if (path.isEmpty() || path.endsWith("/")) {
            path += "index.html"
        }

        val assetContents = try {
            val assetManager = InstrumentationRegistry.getInstrumentation().context.assets
            assetManager.open(path).use { inputStream ->
                inputStream.bufferedReader().use { it.readText() }
            }
        } catch (e: IOException) { // e.g. file not found.
            // We're on a background thread so we need to forward the exception to the main thread.
            mainThreadHandler.postAtFrontOfQueue {
                throw IllegalStateException("Could not load resource from path: $path", e)
            }
            return MockResponse().setResponseCode(HTTP_NOT_FOUND)
        }
        return MockResponse().setResponseCode(HTTP_OK).setBody(assetContents)
    }
}
