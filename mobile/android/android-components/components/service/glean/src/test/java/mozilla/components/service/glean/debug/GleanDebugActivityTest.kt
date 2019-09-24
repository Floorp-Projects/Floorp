/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.debug

import android.content.Context
import android.content.Intent
import android.content.pm.ActivityInfo
import android.content.pm.ResolveInfo
import androidx.test.core.app.ApplicationProvider
import mozilla.components.service.glean.Glean
import mozilla.components.service.glean.TestPingTagClient
import mozilla.components.service.glean.config.Configuration
import mozilla.components.service.glean.getMockWebServer
import mozilla.components.service.glean.net.ConceptFetchHttpUploader
import mozilla.components.service.glean.private.BooleanMetricType
import mozilla.components.service.glean.private.Lifetime
import mozilla.components.service.glean.resetGlean
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.service.glean.triggerWorkManager
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner
import org.robolectric.Shadows.shadowOf
import java.util.concurrent.TimeUnit

@RunWith(RobolectricTestRunner::class)
class GleanDebugActivityTest {

    private val testPackageName = "mozilla.components.service.glean"

    @get:Rule
    val gleanRule = GleanTestRule(ApplicationProvider.getApplicationContext())

    @Before
    fun setup() {
        // This makes sure we have a "launch" intent in our package, otherwise
        // it will fail looking for it in `GleanDebugActivityTest`.
        val pm = ApplicationProvider.getApplicationContext<Context>().packageManager
        val launchIntent = Intent(Intent.ACTION_MAIN)
        launchIntent.setPackage(testPackageName)
        launchIntent.addCategory(Intent.CATEGORY_LAUNCHER)
        val resolveInfo = ResolveInfo()
        resolveInfo.activityInfo = ActivityInfo()
        resolveInfo.activityInfo.packageName = testPackageName
        resolveInfo.activityInfo.name = "LauncherActivity"
        @Suppress("DEPRECATION")
        shadowOf(pm).addResolveInfoForIntent(launchIntent, resolveInfo)
    }

    @Test
    fun `the default configuration is not changed if no extras are provided`() {
        val originalConfig = Configuration()
        Glean.configuration = originalConfig

        // Build the intent that will call our debug activity, with no extra.
        val intent = Intent(ApplicationProvider.getApplicationContext<Context>(),
            GleanDebugActivity::class.java)
        assertNull(intent.extras)

        // Start the activity through our intent.
        val activity = Robolectric.buildActivity(GleanDebugActivity::class.java, intent)
        activity.create().start().resume()

        // Verify that the original configuration and the one after init took place
        // are the same.
        assertEquals(originalConfig, Glean.configuration)
    }

    @Test
    fun `command line extra arguments are correctly parsed`() {
        // Make sure to set a baseline configuration to check against.
        val originalConfig = Configuration()
        Glean.configuration = originalConfig
        assertFalse(originalConfig.logPings)

        // Set the extra values and start the intent.
        val intent = Intent(ApplicationProvider.getApplicationContext<Context>(),
            GleanDebugActivity::class.java)
        intent.putExtra(GleanDebugActivity.LOG_PINGS_EXTRA_KEY, true)
        val activity = Robolectric.buildActivity(GleanDebugActivity::class.java, intent)
        activity.create().start().resume()

        // Check that the configuration option was correctly flipped.
        assertTrue(Glean.configuration.logPings)
    }

    @Test
    fun `the main activity is correctly started`() {
        // Build the intent that will call our debug activity, with no extra.
        val intent = Intent(ApplicationProvider.getApplicationContext<Context>(),
            GleanDebugActivity::class.java)
        // Add at least an option, otherwise the activity will be removed.
        intent.putExtra(GleanDebugActivity.LOG_PINGS_EXTRA_KEY, true)
        // Start the activity through our intent.
        val activity = Robolectric.buildActivity(GleanDebugActivity::class.java, intent)
        activity.create().start().resume()

        // Check that our main activity was launched.
        assertEquals(testPackageName,
            shadowOf(activity.get()).peekNextStartedActivityForResult().intent.`package`!!)
    }

    @Test
    fun `pings are sent using sendPing`() {
        val server = getMockWebServer()

        resetGlean(config = Glean.configuration.copy(
            serverEndpoint = "http://" + server.hostName + ":" + server.port
        ))

        // Put some metric data in the store, otherwise we won't get a ping out
        // Define a 'booleanMetric' boolean metric, which will be stored in "store1"
        val booleanMetric = BooleanMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "boolean_metric",
            sendInPings = listOf("metrics")
        )

        booleanMetric.set(true)
        assertTrue(booleanMetric.testHasValue())

        // Set the extra values and start the intent.
        val intent = Intent(ApplicationProvider.getApplicationContext<Context>(),
        GleanDebugActivity::class.java)
        intent.putExtra(GleanDebugActivity.SEND_PING_EXTRA_KEY, "metrics")
        val activity = Robolectric.buildActivity(GleanDebugActivity::class.java, intent)
        activity.create().start().resume()

        // Since we reset the serverEndpoint back to the default for untagged pings, we need to
        // override it here so that the local server we created to intercept the pings will
        // be the one that the ping is sent to.
        Glean.configuration = Glean.configuration.copy(
            serverEndpoint = "http://" + server.hostName + ":" + server.port
        )

        triggerWorkManager()
        val request = server.takeRequest(10L, TimeUnit.SECONDS)

        assertTrue(
            request.requestUrl.encodedPath().startsWith("/submit/mozilla-components-service-glean/metrics")
        )

        server.shutdown()
    }

    @Test
    fun `tagPings filters ID's that don't match the pattern`() {
        val server = getMockWebServer()

        resetGlean(config = Glean.configuration.copy(
            serverEndpoint = "http://" + server.hostName + ":" + server.port
        ))

        // Put some metric data in the store, otherwise we won't get a ping out
        // Define a 'booleanMetric' boolean metric, which will be stored in "store1"
        val booleanMetric = BooleanMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "boolean_metric",
            sendInPings = listOf("metrics")
        )

        booleanMetric.set(true)
        assertTrue(booleanMetric.testHasValue())

        // Set the extra values and start the intent.
        val intent = Intent(ApplicationProvider.getApplicationContext<Context>(),
            GleanDebugActivity::class.java)
        intent.putExtra(GleanDebugActivity.SEND_PING_EXTRA_KEY, "metrics")
        intent.putExtra(GleanDebugActivity.TAG_DEBUG_VIEW_EXTRA_KEY, "inv@lid_id")
        val activity = Robolectric.buildActivity(GleanDebugActivity::class.java, intent)
        activity.create().start().resume()

        // Since a bad tag ID results in resetting the endpoint to the default, verify that
        // has happened.
        assertEquals("Server endpoint must be reset if tag didn't pass regex",
            "http://" + server.hostName + ":" + server.port, Glean.configuration.serverEndpoint)

        triggerWorkManager()
        val request = server.takeRequest(10L, TimeUnit.SECONDS)

        assertTrue(
            "Request path must be correct",
            request.requestUrl.encodedPath().startsWith("/submit/mozilla-components-service-glean/metrics")
        )

        assertNull(
            "Headers must not contain X-Debug-ID if passed a non matching pattern",
            request.headers.get("X-Debug-ID")
        )

        server.shutdown()
    }

    @Test
    fun `pings are correctly tagged using tagPings`() {
        val pingTag = "test-debug-ID"

        // The TestClient class found in TestUtil is used to intercept the request in order to check
        // that the header has been added correctly for the tagged ping.
        val testClient = TestPingTagClient(
            debugHeaderValue = pingTag)

        // Use the test client in the Glean configuration
        resetGlean(config = Glean.configuration.copy(
            httpClient = ConceptFetchHttpUploader(lazy { testClient })
        ))

        // Put some metric data in the store, otherwise we won't get a ping out
        // Define a 'booleanMetric' boolean metric, which will be stored in "store1"
        val booleanMetric = BooleanMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "boolean_metric",
            sendInPings = listOf("metrics")
        )

        booleanMetric.set(true)
        assertTrue(booleanMetric.testHasValue())

        // Set the extra values and start the intent.
        val intent = Intent(ApplicationProvider.getApplicationContext<Context>(),
            GleanDebugActivity::class.java)
        intent.putExtra(GleanDebugActivity.SEND_PING_EXTRA_KEY, "metrics")
        intent.putExtra(GleanDebugActivity.TAG_DEBUG_VIEW_EXTRA_KEY, pingTag)
        val activity = Robolectric.buildActivity(GleanDebugActivity::class.java, intent)
        activity.create().start().resume()

        // This will trigger the call to `fetch()` in the TestPingTagClient which is where the
        // test assertions will occur
        triggerWorkManager()
    }
}
