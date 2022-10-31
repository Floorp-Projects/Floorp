/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.glean.pings

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.rules.ActivityScenarioRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import mozilla.components.service.glean.testing.GleanTestLocalServer
import okhttp3.mockwebserver.Dispatcher
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import okhttp3.mockwebserver.RecordedRequest
import org.json.JSONObject
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.samples.glean.MainActivity
import java.io.BufferedReader
import java.io.ByteArrayInputStream
import java.util.concurrent.TimeUnit
import java.util.zip.GZIPInputStream

/**
 * Decompress the GZIP returned by the glean-core layer.
 *
 * @param data the gzipped [ByteArray] to decompress
 * @return a [String] containing the uncompressed data.
 */
fun decompressGZIP(data: ByteArray): String {
    return GZIPInputStream(ByteArrayInputStream(data)).bufferedReader().use(BufferedReader::readText)
}

/**
 * Convenience method to get the body of a request as a String.
 * The UTF8 representation of the request body will be returned.
 * If the request body is gzipped, it will be decompressed first.
 *
 * @return a [String] containing the body of the request.
 */
fun RecordedRequest.getPlainBody(): String {
    return if (this.getHeader("Content-Encoding") == "gzip") {
        val bodyInBytes = this.body.readByteArray()
        decompressGZIP(bodyInBytes)
    } else {
        this.body.readUtf8()
    }
}

@RunWith(AndroidJUnit4::class)
class BaselinePingTest {
    private val server = createMockWebServer()

    @get:Rule
    val activityRule: ActivityScenarioRule<MainActivity> = ActivityScenarioRule(MainActivity::class.java)

    @get:Rule
    val gleanRule = GleanTestLocalServer(context, server.port)

    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    /**
     * Create a mock webserver that accepts all requests and replies with "OK".
     * @return a [MockWebServer] instance
     */
    private fun createMockWebServer(): MockWebServer {
        val server = MockWebServer()
        server.setDispatcher(
            object : Dispatcher() {
                override fun dispatch(request: RecordedRequest): MockResponse {
                    return MockResponse().setBody("OK")
                }
            },
        )
        return server
    }

    private fun waitForPingContent(
        pingName: String,
        pingReason: String?,
        maxAttempts: Int = 3,
    ): JSONObject? {
        var attempts = 0
        do {
            attempts += 1
            val request = server.takeRequest(20L, TimeUnit.SECONDS)
            val docType = request.path.split("/")[3]
            if (pingName == docType) {
                val parsedPayload = JSONObject(request.getPlainBody())
                if (pingReason == null) {
                    return parsedPayload
                }

                // If we requested a specific ping reason, look for it.
                val reason = parsedPayload.getJSONObject("ping_info").getString("reason")
                if (reason == pingReason) {
                    return parsedPayload
                }
            }
        } while (attempts < maxAttempts)

        return null
    }

    @Test
    fun validateBaselinePing() {
        // Wait for the app to be idle/ready.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync()
        val device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        device.waitForIdle()

        // Wait for 1 second: this should guarantee we have some valid duration in the
        // ping.
        Thread.sleep(1000)

        // Move it to background.
        device.pressHome()

        // Validate the received data.
        val baselinePing = waitForPingContent("baseline", "inactive")!!
        val metrics = baselinePing.getJSONObject("metrics")

        // Make sure we have a 'duration' field with a reasonable value: it should be >= 1, since
        // we slept for 1000ms.
        val timespans = metrics.getJSONObject("timespan")
        assertTrue(timespans.getJSONObject("glean.baseline.duration").getLong("value") >= 1L)

        // Make sure there's no errors.
        val errors = metrics.optJSONObject("labeled_counter")?.keys()
        errors?.forEach {
            assertFalse(it.startsWith("glean.error."))
        }
    }
}
