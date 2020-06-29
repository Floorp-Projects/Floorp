/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.content.Intent
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.RelaxedMockK
import io.mockk.verify
import mozilla.components.support.utils.toSafeIntent
import org.junit.Before
import org.junit.Test

class AppAllSourceStartTelemetryTest {

    @RelaxedMockK
    private lateinit var metricController: MetricController

    @RelaxedMockK
    private lateinit var intent: Intent

    private lateinit var appAllSourceStartTelemetry: AppAllSourceStartTelemetry

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        appAllSourceStartTelemetry = AppAllSourceStartTelemetry(metricController)
    }

    @Test
    fun `WHEN a main launcher intent is received in HomeActivity THEN an app start metric is recorded from app_icon`() {

        every { intent.action } returns Intent.ACTION_MAIN
        every { intent.categories.contains(Intent.CATEGORY_LAUNCHER) } returns true

        appAllSourceStartTelemetry.receivedIntentInHomeActivity(intent.toSafeIntent())

        val validSource = Event.AppOpenedAllSourceStartup.Source.APP_ICON
        verify(exactly = 1) { metricController.track(Event.AppOpenedAllSourceStartup(validSource)) }
    }

    @Test
    fun `WHEN a VIEW intent is received in HomeActivity THEN an app start metric is recorded from link`() {
        every { intent.action } returns Intent.ACTION_VIEW

        appAllSourceStartTelemetry.receivedIntentInHomeActivity(intent.toSafeIntent())

        val validSource = Event.AppOpenedAllSourceStartup.Source.LINK
        verify(exactly = 1) { metricController.track(Event.AppOpenedAllSourceStartup(validSource)) }
    }

    @Test
    fun `WHEN a intent is received in ExternalAppBrowserActivity THEN an app start metric is recorded from custom_tab`() {
        val intent = Intent()

        appAllSourceStartTelemetry.receivedIntentInExternalAppBrowserActivity(intent.toSafeIntent())

        val validSource = Event.AppOpenedAllSourceStartup.Source.CUSTOM_TAB
        verify(exactly = 1) { metricController.track(Event.AppOpenedAllSourceStartup(validSource)) }
    }

    @Test
    fun `GIVEN an app is in the foreground WHEN an intent is received THEN no startup metric is recorded`() {
        appAllSourceStartTelemetry.receivedIntentInHomeActivity(intent.toSafeIntent())

        appAllSourceStartTelemetry.receivedIntentInHomeActivity(intent.toSafeIntent())

        verify(exactly = 1) { metricController.track(any()) }
    }

    @Test
    fun `WHEN application goes in background and comes foreground, THEN an app start metric is recorded`() {
        // first startup
        appAllSourceStartTelemetry.receivedIntentInHomeActivity(intent.toSafeIntent())

        // mock application going in the background
        appAllSourceStartTelemetry.onApplicationOnStop()

        appAllSourceStartTelemetry.receivedIntentInHomeActivity(intent.toSafeIntent())

        verify(exactly = 2) { metricController.track(any()) }
    }

    @Test
    fun `WHEN an intent received in HomeActivity is not launcher or does not have VIEW action, THEN an app start is recorded from unknown`() {
        every { intent.action } returns Intent.ACTION_MAIN

        appAllSourceStartTelemetry.receivedIntentInHomeActivity(intent.toSafeIntent())

        val validSource = Event.AppOpenedAllSourceStartup.Source.UNKNOWN
        verify(exactly = 1) { metricController.track(Event.AppOpenedAllSourceStartup(validSource)) }
    }
}
