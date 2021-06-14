/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.glean

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.rule.ActivityTestRule
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiSelector
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import mozilla.components.browser.engine.gecko.fetch.GeckoViewFetchClient
import mozilla.components.service.glean.Glean
import mozilla.components.service.glean.config.Configuration
import mozilla.components.service.glean.net.ConceptFetchHttpUploader
import mozilla.components.service.glean.testing.GleanTestLocalServer
import okhttp3.mockwebserver.RecordedRequest
import org.json.JSONObject
import org.junit.Assert.assertTrue
import org.junit.Assert.assertFalse
import org.junit.BeforeClass
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.GleanBuildInfo
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.MockWebServerHelper
import java.util.concurrent.TimeUnit
import java.io.BufferedReader
import java.io.ByteArrayInputStream
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
    private val server = MockWebServerHelper.createAlwaysOkMockWebServer()

    @get:Rule
    val activityRule: ActivityTestRule<HomeActivity> = HomeActivityTestRule()

    @get:Rule
    val gleanRule = GleanTestLocalServer(ApplicationProvider.getApplicationContext(), server.port)

    companion object {
        @BeforeClass
        @JvmStatic
        @OptIn(DelicateCoroutinesApi::class) // GlobalScope usage
        fun setupOnce() {
            val httpClient = ConceptFetchHttpUploader(lazy {
                GeckoViewFetchClient(ApplicationProvider.getApplicationContext())
            })

            // Fenix does not initialize the Glean SDK in tests/debug builds, but this test
            // requires Glean to be initialized so we need to do it manually. Additionally,
            // we need to do this on the main thread, as the Glean SDK requires it.
            GlobalScope.launch(Dispatchers.Main.immediate) {
                Glean.initialize(
                    applicationContext = ApplicationProvider.getApplicationContext(),
                    uploadEnabled = true,
                    configuration = Configuration(httpClient = httpClient),
                    buildInfo = GleanBuildInfo.buildInfo
                )
            }
        }
    }

    /**
     * Wait for a specific ping to be received by the local server and
     * return its parsed JSON content.
     *
     * @param pingName the name of the ping to wait for
     * @param pingReason the value of the `reason` field for the received ping
     * @param maxAttempts how many times should a wait be attempted
     */
    private fun waitForPingContent(
        pingName: String,
        pingReason: String?,
        maxAttempts: Int = 3
    ): JSONObject? {
        var attempts = 0
        do {
            attempts += 1
            val request = server.takeRequest(20L, TimeUnit.SECONDS) ?: break
            val docType = request.path!!.split("/")[3]
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

        // Due to bug 1632184, we need move the activity to foreground again, in order
        // for a 'background' ping with reason 'foreground' to be generated and also trigger
        // sending the ping that was submitted on background. This can go away once bug 1634375
        // is fixed.
        device.pressRecentApps()
        device.findObject(UiSelector().descriptionContains(
            ApplicationProvider.getApplicationContext<Context>().getString(R.string.app_name)))
            .click()

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
