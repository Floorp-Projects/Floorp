/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.glean

import androidx.annotation.VisibleForTesting
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.runner.AndroidJUnitRunner
import okhttp3.mockwebserver.Dispatcher
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import okhttp3.mockwebserver.RecordedRequest

/**
 * Create a mock webserver that accepts all requests and replies with "OK".
 * @return a [MockWebServer] instance
 */
private fun createMockWebServer(): MockWebServer {
    val server = MockWebServer()
    server.setDispatcher(object : Dispatcher() {
        override fun dispatch(request: RecordedRequest): MockResponse {
            return MockResponse().setBody("OK")
        }
    })
    return server
}

/**
 * Returns the currently active instance of the ping server.
 *
 * @return the active [MockWebServer] instance
 */
@VisibleForTesting(otherwise = VisibleForTesting.NONE)
internal fun getPingServer(): MockWebServer {
    val testRunner: GleanTestRunner = InstrumentationRegistry.getInstrumentation() as GleanTestRunner
    return testRunner.pingServer
}

/**
 * Returns the address the local ping server is listening to.
 *
 * @return a `String` containing the server address.
 */
@VisibleForTesting(otherwise = VisibleForTesting.NONE)
internal fun getPingServerAddress(): String {
    val testRunner: GleanTestRunner = InstrumentationRegistry.getInstrumentation() as GleanTestRunner
    return testRunner.pingServerAddress!!
}

/**
 * The test runner to be used in the instrumentation tests for the Glean SDK
 * sample app in order to point it to a local ping server.
 */
@VisibleForTesting(otherwise = VisibleForTesting.NONE)
class GleanTestRunner : AndroidJUnitRunner() {
    // Add a lazy ping server to the app runner. This is only initialized once
    // since the `Application` object is re-used.
    internal val pingServer: MockWebServer by lazy { createMockWebServer() }
    internal var pingServerAddress: String? = null

    init {
        // We need to start the server off the main thread, otherwise
        // Android will throw a `NetworkOnMainThreadException`. Spawning
        // a thread and joining seems fine.
        val thread = Thread {
            pingServer.start()
            pingServerAddress = "http://${pingServer.hostName}:${pingServer.port}"
        }
        thread.start()
        thread.join()
    }

    /**
     * Called before the application code runs, this starts the ping server.
     */
    override fun onStart() {
        super.onStart()
        pingServer.start()
    }

    /**
     * Called after the application code cleans up, this stops the ping server.
     */
    override fun onDestroy() {
        super.onDestroy()
        pingServer.shutdown()
    }
}
