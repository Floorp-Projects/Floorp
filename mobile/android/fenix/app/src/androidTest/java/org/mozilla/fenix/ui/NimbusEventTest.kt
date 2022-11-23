package org.mozilla.fenix.ui

import android.content.Intent
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import mozilla.components.concept.sync.*
import okhttp3.mockwebserver.MockWebServer
import org.junit.*
import org.mozilla.fenix.components.TelemetryAccountObserver
import org.mozilla.fenix.helpers.*
import org.mozilla.fenix.helpers.TestHelper.appContext
import org.mozilla.fenix.ui.robots.homeScreen
import io.mockk.mockk

class NimbusEventTest {
    private lateinit var mDevice: UiDevice
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val homeActivityTestRule = HomeActivityIntentTestRule.withDefaultSettingsOverrides()
        .withIntent(Intent().apply {
            action = Intent.ACTION_VIEW
        })

    @Rule
    @JvmField
    val retryTestRule = RetryTestRule(3)

    @Before
    fun setUp() {
        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
    }

    @Test
    fun homeScreenNimbusEventsTest() {
        homeScreen { }.dismissOnboarding()

        Experimentation.withHelper {
            Assert.assertTrue(evalJexl("'app_opened'|eventSum('Days', 28, 0) > 0"))
        }
    }

    @Test
    fun telemetryAccountObserverTest() {
        val observer = TelemetryAccountObserver(appContext)
        observer.onAuthenticated(mockk(), AuthType.Signin)

        Experimentation.withHelper {
            Assert.assertTrue(evalJexl("'sync_auth_sign_in'|eventSum('Days', 28, 0) > 0"))
        }
    }
}
