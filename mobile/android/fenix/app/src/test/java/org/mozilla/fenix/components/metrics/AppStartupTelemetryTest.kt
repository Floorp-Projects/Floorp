/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.content.Intent
import io.mockk.MockKAnnotations
import io.mockk.clearMocks
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.verify
import mozilla.components.support.utils.toSafeIntent
import org.junit.Before
import org.junit.Test
import org.junit.Assert.assertTrue
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.Events.appOpenedAllStartupKeys.hasSavedInstanceState
import org.mozilla.fenix.GleanMetrics.Events.appOpenedAllStartupKeys.source
import org.mozilla.fenix.GleanMetrics.Events.appOpenedAllStartupKeys.type
import org.mozilla.fenix.components.metrics.Event.AppAllStartup
import org.mozilla.fenix.components.metrics.Event.AppAllStartup.Source
import org.mozilla.fenix.components.metrics.Event.AppAllStartup.Source.APP_ICON
import org.mozilla.fenix.components.metrics.Event.AppAllStartup.Source.UNKNOWN
import org.mozilla.fenix.components.metrics.Event.AppAllStartup.Source.CUSTOM_TAB
import org.mozilla.fenix.components.metrics.Event.AppAllStartup.Source.LINK
import org.mozilla.fenix.components.metrics.Event.AppAllStartup.Type.COLD
import org.mozilla.fenix.components.metrics.Event.AppAllStartup.Type.WARM
import org.mozilla.fenix.components.metrics.Event.AppAllStartup.Type.HOT
import org.mozilla.fenix.components.metrics.Event.AppAllStartup.Type.ERROR

class AppStartupTelemetryTest {

    @MockK
    private lateinit var metricController: MetricController
    @MockK
    private lateinit var intent: Intent

    private lateinit var appStartupTelemetry: AppStartupTelemetry

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        appStartupTelemetry = AppStartupTelemetry(metricController)
        every { metricController.track(any()) } returns Unit
    }

    @Test
    fun `WHEN application is launch for the first time through application icon THEN records the correct values`() {
        setupIntentMock(APP_ICON)

        appStartupTelemetry.onFenixApplicationOnCreate()
        appStartupTelemetry.onHomeActivityOnCreate(intent.toSafeIntent(), false)
        appStartupTelemetry.onHomeActivityOnResume()

        val validMetric = AppAllStartup(APP_ICON, COLD, false)
        verify(exactly = 1) { metricController.track(validMetric) }
    }

    @Test
    fun `WHEN application is launch for the first time through a url link THEN records the correct values`() {
        setupIntentMock(LINK)

        appStartupTelemetry.onFenixApplicationOnCreate()
        appStartupTelemetry.onHomeActivityOnCreate(intent.toSafeIntent(), false)
        appStartupTelemetry.onHomeActivityOnResume()

        val validMetric = AppAllStartup(LINK, COLD, false)
        verify(exactly = 1) { metricController.track(validMetric) }
    }

    @Test
    fun `WHEN application is launch for the first time through an custom tab THEN records the correct values`() {
        setupIntentMock(CUSTOM_TAB)

        appStartupTelemetry.onFenixApplicationOnCreate()
        appStartupTelemetry.onExternalAppBrowserOnCreate(intent.toSafeIntent(), false)
        appStartupTelemetry.onHomeActivityOnResume()

        val validMetric = AppAllStartup(CUSTOM_TAB, COLD, false)
        verify(exactly = 1) { metricController.track(validMetric) }
    }

    @Test
    fun `GIVEN, application  exists and is backgrounded, WHEN application is launched again through app icon and HomeActivity is recreated THEN records the correct values`() {
        setupIntentMock(APP_ICON)
        launchApplicationAndPutApplicationInBackground()

        appStartupTelemetry.onHomeActivityOnCreate(intent.toSafeIntent(), false)
        appStartupTelemetry.onHomeActivityOnResume()

        val validMetric = AppAllStartup(APP_ICON, WARM, false)
        verify(exactly = 1) { metricController.track(validMetric) }
    }

    @Test
    fun `GIVEN, application  exists and is backgrounded, WHEN application is launched again through url link and HomeActivity is recreated THEN records the correct values`() {
        setupIntentMock(LINK)
        launchApplicationAndPutApplicationInBackground()

        appStartupTelemetry.onHomeActivityOnCreate(intent.toSafeIntent(), false)
        appStartupTelemetry.onHomeActivityOnResume()

        val validMetric = AppAllStartup(LINK, WARM, false)
        verify(exactly = 1) { metricController.track(validMetric) }
    }

    @Test
    fun `GIVEN, application  exists and is backgrounded, WHEN application is launched again through custom tab and ExternalAppBrowserActivity is recreated THEN records the correct values`() {
        setupIntentMock(CUSTOM_TAB)
        launchApplicationAndPutApplicationInBackground()

        appStartupTelemetry.onExternalAppBrowserOnCreate(intent.toSafeIntent(), false)
        appStartupTelemetry.onHomeActivityOnResume()

        val validMetric = AppAllStartup(CUSTOM_TAB, WARM, false)
        verify(exactly = 1) { metricController.track(validMetric) }
    }

    @Test
    fun `GIVEN, application  exists and is backgrounded, WHEN application is launched again through app icon and HomeActivity is restarted THEN records the correct values`() {
        setupIntentMock(APP_ICON)
        launchApplicationAndPutApplicationInBackground()

        appStartupTelemetry.onHomeActivityOnRestart()
        appStartupTelemetry.onHomeActivityOnNewIntent(intent.toSafeIntent())
        appStartupTelemetry.onHomeActivityOnResume()

        val validMetric = AppAllStartup(APP_ICON, HOT)
        verify(exactly = 1) { metricController.track(validMetric) }
    }

    @Test
    fun `GIVEN, application  exists and is backgrounded, WHEN application is launched again through url link and HomeActivity is restarted THEN records the correct values`() {
        setupIntentMock(LINK)
        launchApplicationAndPutApplicationInBackground()

        appStartupTelemetry.onHomeActivityOnRestart()
        appStartupTelemetry.onHomeActivityOnNewIntent(intent.toSafeIntent())
        appStartupTelemetry.onHomeActivityOnResume()

        val validMetric = AppAllStartup(LINK, HOT)
        verify(exactly = 1) { metricController.track(validMetric) }
    }

    @Test
    fun `WHEN application is launched and onResume() is called twice THEN metric is reported only once`() {
        setupIntentMock(LINK)
        appStartupTelemetry.onExternalAppBrowserOnCreate(intent.toSafeIntent(), false)
        appStartupTelemetry.onHomeActivityOnResume()

        appStartupTelemetry.onHomeActivityOnResume()

        verify(exactly = 1) { metricController.track(any()) }
    }

    @Test
    fun `GIVEN application is in background WHEN application is launched again through unknown source and HomeActivity exists THEN records the correct values`() {
        setupIntentMock(UNKNOWN)
        launchApplicationAndPutApplicationInBackground()

        appStartupTelemetry.onHomeActivityOnCreate(intent.toSafeIntent(), false)
        appStartupTelemetry.onHomeActivityOnResume()

        val validMetric = AppAllStartup(UNKNOWN, WARM, false)
        verify(exactly = 1) { metricController.track(validMetric) }
    }

    @Test
    fun `GIVEN application exists and is backgrounded WHEN application started again through app icon but HomeActivity is recreated from savedInstanceState THEN records the correct values`() {
        setupIntentMock(APP_ICON)
        launchApplicationAndPutApplicationInBackground()

        appStartupTelemetry.onHomeActivityOnCreate(intent.toSafeIntent(), true)
        appStartupTelemetry.onHomeActivityOnResume()

        val validMetric = AppAllStartup(APP_ICON, WARM, true)
        verify(exactly = 1) { metricController.track(validMetric) }
    }

    private fun launchApplicationAndPutApplicationInBackground() {
        appStartupTelemetry.onFenixApplicationOnCreate()
        appStartupTelemetry.onHomeActivityOnCreate(intent.toSafeIntent(), false)
        appStartupTelemetry.onHomeActivityOnResume()

        // have to clear the mock function calls so it doesnt interfere with tests
        clearMocks(metricController, answers = false)

        appStartupTelemetry.onApplicationOnStop()
    }

    @Test
    fun `GIVEN application is in background WHEN application is launched again HomeActivity only calls onResume THEN records the correct values`() {
        setupIntentMock(UNKNOWN)
        launchApplicationAndPutApplicationInBackground()

        appStartupTelemetry.onHomeActivityOnResume()

        val validMetric = AppAllStartup(UNKNOWN, ERROR)
        verify(exactly = 1) { metricController.track(validMetric) }
    }

    @Test
    fun `GIVEN application is in background WHEN application is launched again HomeActivity calls onRestart but not onNewIntent THEN records the correct values`() {
        setupIntentMock(APP_ICON)
        launchApplicationAndPutApplicationInBackground()

        appStartupTelemetry.onHomeActivityOnRestart()
        appStartupTelemetry.onHomeActivityOnResume()

        val validMetric = AppAllStartup(UNKNOWN, HOT)
        verify(exactly = 1) { metricController.track(validMetric) }
    }

    @Test
    fun `GIVEN application is in background WHEN application is launched again and HomeActivity calls onNewIntent but not onRestart THEN records the correct values`() {
        setupIntentMock(APP_ICON)
        launchApplicationAndPutApplicationInBackground()

        appStartupTelemetry.onHomeActivityOnNewIntent(intent.toSafeIntent())
        appStartupTelemetry.onHomeActivityOnResume()

        val validMetric = AppAllStartup(APP_ICON, ERROR)
        verify(exactly = 1) { metricController.track(validMetric) }
    }

    @Test
    fun `WHEN AppAllStartup does not have savedInstanceState THEN do not return savedInstanceState`() {
        val expectedExtra: Map<Events.appOpenedAllStartupKeys, String>? = hashMapOf(
            source to APP_ICON.toString(),
            type to HOT.toString())

        val appAllStartup = AppAllStartup(APP_ICON, HOT)

        assertTrue(appAllStartup.extras!! == expectedExtra)
    }

    @Test
    fun `WHEN AppAllStartup have savedInstanceState THEN return savedInstanceState `() {
        val expectedExtra: Map<Events.appOpenedAllStartupKeys, String>? = hashMapOf(
            source to APP_ICON.toString(),
            type to COLD.toString(),
            hasSavedInstanceState to true.toString())

        val appAllStartup = AppAllStartup(APP_ICON, COLD, true)

        assertTrue(appAllStartup.extras!! == expectedExtra)
    }

    private fun setupIntentMock(source: Source) {
        when (source) {
            APP_ICON -> {
                every { intent.action } returns Intent.ACTION_MAIN
                every { intent.categories } returns setOf(Intent.CATEGORY_LAUNCHER)
            }
            LINK, CUSTOM_TAB -> {
                every { intent.action } returns Intent.ACTION_VIEW
                every { intent.categories } returns emptySet()
            }
            UNKNOWN -> {
                every { intent.action } returns Intent.ACTION_MAIN
                every { intent.categories } returns emptySet()
            }
        }
    }
}
