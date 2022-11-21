package org.mozilla.fenix.ui

import android.content.Intent
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import okhttp3.mockwebserver.MockWebServer
import org.junit.*
import org.mozilla.fenix.helpers.*
import org.mozilla.fenix.ui.robots.homeScreen

class NimbusEventTest {
    private lateinit var mDevice: UiDevice
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val activityTestRule = HomeActivityIntentTestRule.withDefaultSettingsOverrides()
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
}
