package mozilla.components.service.glean.debug

import android.content.Context
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import android.content.Intent
import androidx.test.core.app.ApplicationProvider
import mozilla.components.service.glean.Glean
import mozilla.components.service.glean.config.Configuration
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.robolectric.Robolectric
import android.content.pm.ActivityInfo
import android.content.pm.ResolveInfo
import androidx.work.testing.WorkManagerTestInitHelper
import mozilla.components.service.glean.metrics.BooleanMetricType
import mozilla.components.service.glean.metrics.Lifetime
import mozilla.components.service.glean.resetGlean
import mozilla.components.service.glean.triggerWorkManager
import mozilla.components.service.glean.TestPingTagClient
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.robolectric.Shadows.shadowOf
import java.util.concurrent.TimeUnit

@RunWith(RobolectricTestRunner::class)
class GleanDebugActivityTest {

    private val testPackageName = "mozilla.components.service.glean"

    @Before
    fun setup() {
        resetGlean()

        WorkManagerTestInitHelper.initializeTestWorkManager(
            ApplicationProvider.getApplicationContext())

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
        // Start the activity through our intent.
        val activity = Robolectric.buildActivity(GleanDebugActivity::class.java, intent)
        activity.create().start().resume()

        // Check that our main activity was launched.
        assertEquals(testPackageName,
            shadowOf(activity.get()).peekNextStartedActivityForResult().intent.`package`!!)
    }

    @Test
    fun `pings are sent using sendPing`() {
        val server = MockWebServer()
        server.enqueue(MockResponse().setBody("OK"))

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
            sendInPings = listOf("store1")
        )

        booleanMetric.set(true)
        assertTrue(booleanMetric.testHasValue())

        // Set the extra values and start the intent.
        val intent = Intent(ApplicationProvider.getApplicationContext<Context>(),
        GleanDebugActivity::class.java)
        intent.putExtra(GleanDebugActivity.SEND_PING_EXTRA_KEY, "store1")
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
            request.requestUrl.encodedPath().startsWith("/submit/mozilla-components-service-glean/store1")
        )

        server.shutdown()
    }

    @Test
    fun `tagPings filters ID's that don't match the pattern`() {
        val server = MockWebServer()
        server.enqueue(MockResponse().setBody("OK"))

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
            sendInPings = listOf("store1")
        )

        booleanMetric.set(true)
        assertTrue(booleanMetric.testHasValue())

        // Set the extra values and start the intent.
        val intent = Intent(ApplicationProvider.getApplicationContext<Context>(),
            GleanDebugActivity::class.java)
        intent.putExtra(GleanDebugActivity.SEND_PING_EXTRA_KEY, "store1")
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
            request.requestUrl.encodedPath().startsWith("/submit/mozilla-components-service-glean/store1")
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

        // The TestClient class found at the bottom of this file is used to intercept the request
        // in order to check that the header has been added and the URL has been redirected.
        val testClient = TestPingTagClient(
            responseUrl = Configuration.DEFAULT_DEBUGVIEW_ENDPOINT,
            debugHeaderValue = pingTag)

        // Use the test client in the Glean configuration
        resetGlean(config = Glean.configuration.copy(
            httpClient = lazy { testClient }
        ))

        // Put some metric data in the store, otherwise we won't get a ping out
        // Define a 'booleanMetric' boolean metric, which will be stored in "store1"
        val booleanMetric = BooleanMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "boolean_metric",
            sendInPings = listOf("store1")
        )

        booleanMetric.set(true)
        assertTrue(booleanMetric.testHasValue())

        // Set the extra values and start the intent.
        val intent = Intent(ApplicationProvider.getApplicationContext<Context>(),
            GleanDebugActivity::class.java)
        intent.putExtra(GleanDebugActivity.SEND_PING_EXTRA_KEY, "store1")
        intent.putExtra(GleanDebugActivity.TAG_DEBUG_VIEW_EXTRA_KEY, pingTag)
        val activity = Robolectric.buildActivity(GleanDebugActivity::class.java, intent)
        activity.create().start().resume()

        // This will trigger the call to `fetch()` in the TestPingTagClient which is where the
        // test assertions will occur
        triggerWorkManager()
    }
}
