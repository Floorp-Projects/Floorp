/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.glean

import androidx.test.core.app.ApplicationProvider
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.rule.ActivityTestRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals

import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import androidx.test.uiautomator.UiDevice
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import mozilla.components.service.glean.Glean
import mozilla.components.service.glean.config.Configuration
import mozilla.components.service.glean.testing.GleanTestLocalServer
import org.json.JSONObject
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.BeforeClass
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.MockWebServerHelper
import java.util.concurrent.TimeUnit

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
        fun setupOnce() {
            // Fenix does not initialize the Glean SDK in tests/debug builds, but this test
            // requires Glean to be initialized so we need to do it manually. Additionally,
            // we need to do this on the main thread, as the Glean SDK requires it.
            GlobalScope.launch(Dispatchers.Main.immediate) {
                Glean.initialize(
                    ApplicationProvider.getApplicationContext(),
                    true,
                    Configuration()
                )
            }
        }
    }

    private fun waitForPingContent(
        pingName: String,
        maxAttempts: Int = 3
    ): JSONObject? {
        var attempts = 0
        do {
            attempts += 1
            val request = server.takeRequest(20L, TimeUnit.SECONDS)
            val docType = request.path.split("/")[3]
            if (pingName == docType) {
                return JSONObject(request.body.readUtf8())
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
        val baselinePing = waitForPingContent("baseline")!!
        assertEquals("baseline", baselinePing.getJSONObject("ping_info")["ping_type"])

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
