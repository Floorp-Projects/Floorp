/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.content.Intent
import androidx.lifecycle.Lifecycle
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.perf.StartupPathProvider.StartupPath

class StartupPathProviderTest {

    private lateinit var provider: StartupPathProvider
    private lateinit var callbacks: StartupPathProvider.StartupPathLifecycleObserver

    @MockK private lateinit var intent: Intent

    @Before
    fun setUp() {
        MockKAnnotations.init(this)

        provider = StartupPathProvider()
        callbacks = provider.getTestCallbacks()
    }

    @Test
    fun `WHEN attach is called THEN the provider is registered to the lifecycle`() {
        val lifecycle = mockk<Lifecycle>(relaxed = true)
        provider.attachOnActivityOnCreate(lifecycle, null)

        verify { lifecycle.addObserver(any()) }
    }

    @Test
    fun `WHEN calling attach THEN the intent is passed to on intent received`() {
        // With this test, we're basically saying, "attach..." does the same thing as
        // "onIntentReceived" so we don't need to duplicate all the tests we run for
        // "onIntentReceived".
        val spyProvider = spyk(provider)
        every { spyProvider.onIntentReceived(intent) } returns Unit
        spyProvider.attachOnActivityOnCreate(mockk(relaxed = true), intent)

        verify { spyProvider.onIntentReceived(intent) }
    }

    @Test
    fun `GIVEN no intent is received and the activity is not started WHEN getting the start up path THEN it is not set`() {
        assertEquals(StartupPath.NOT_SET, provider.startupPathForActivity)
    }

    @Test
    fun `GIVEN a main intent is received but the activity is not started yet WHEN getting the start up path THEN main is returned`() {
        every { intent.action } returns Intent.ACTION_MAIN
        provider.onIntentReceived(intent)
        assertEquals(StartupPath.MAIN, provider.startupPathForActivity)
    }

    @Test
    fun `GIVEN a main intent is received and the app is started WHEN getting the start up path THEN it is main`() {
        every { intent.action } returns Intent.ACTION_MAIN
        callbacks.onCreate(mockk())
        provider.onIntentReceived(intent)
        callbacks.onStart(mockk())

        assertEquals(StartupPath.MAIN, provider.startupPathForActivity)
    }

    @Test
    fun `GIVEN the app is launched from the homescreen WHEN getting the start up path THEN it is main`() {
        // There's technically more to a homescreen Intent but it's fine for now.
        every { intent.action } returns Intent.ACTION_MAIN
        launchApp(intent)
        assertEquals(StartupPath.MAIN, provider.startupPathForActivity)
    }

    @Test
    fun `GIVEN the app is launched by app link WHEN getting the start up path THEN it is view`() {
        // There's technically more to a homescreen Intent but it's fine for now.
        every { intent.action } returns Intent.ACTION_VIEW
        launchApp(intent)
        assertEquals(StartupPath.VIEW, provider.startupPathForActivity)
    }

    @Test
    fun `GIVEN the app is launched by a send action WHEN getting the start up path THEN it is unknown`() {
        every { intent.action } returns Intent.ACTION_SEND
        launchApp(intent)
        assertEquals(StartupPath.UNKNOWN, provider.startupPathForActivity)
    }

    @Test
    fun `GIVEN the app is launched by a null intent (is this possible) WHEN getting the start up path THEN it is not set`() {
        callbacks.onCreate(mockk())
        provider.onIntentReceived(null)
        callbacks.onStart(mockk())
        callbacks.onResume(mockk())

        assertEquals(StartupPath.NOT_SET, provider.startupPathForActivity)
    }

    @Test
    fun `GIVEN the app is launched to the homescreen with MAIN and stopped WHEN getting the start up path THEN it set to MAIN`() {
        every { intent.action } returns Intent.ACTION_MAIN
        launchApp(intent)
        stopLaunchedApp()

        assertEquals(StartupPath.MAIN, provider.startupPathForActivity)
    }

    @Test
    fun `GIVEN the app is launched to the homescreen, stopped, and relaunched warm from app link WHEN getting the start up path THEN it is view`() {
        every { intent.action } returns Intent.ACTION_MAIN
        launchApp(intent)
        stopLaunchedApp()

        every { intent.action } returns Intent.ACTION_VIEW
        startStoppedApp(intent)

        assertEquals(StartupPath.VIEW, provider.startupPathForActivity)
    }

    @Test
    fun `GIVEN the app is launched to the homescreen with MAIN, stopped, and relaunched warm from the app switcher WHEN getting the start up path THEN it set to MAIN'`() {
        every { intent.action } returns Intent.ACTION_MAIN
        launchApp(intent)
        stopLaunchedApp()
        startStoppedAppFromAppSwitcher()

        assertEquals(StartupPath.MAIN, provider.startupPathForActivity)
    }

    @Test
    fun `GIVEN the app is launched to the homescreen, paused, and resumed WHEN getting the start up path THEN it returns the initial intent value`() {
        every { intent.action } returns Intent.ACTION_MAIN
        launchApp(intent)
        callbacks.onPause(mockk())
        callbacks.onResume(mockk())

        assertEquals(StartupPath.MAIN, provider.startupPathForActivity)
    }

    @Test
    fun `GIVEN the app is launched with an intent and receives an intent while the activity is foregrounded WHEN getting the start up path THEN it returns the initial intent value`() {
        every { intent.action } returns Intent.ACTION_MAIN
        launchApp(intent)
        every { intent.action } returns Intent.ACTION_VIEW
        receiveIntentInForeground(intent)

        assertEquals(StartupPath.MAIN, provider.startupPathForActivity)
    }

    @Test
    fun `GIVEN the app is launched with MAIN, stopped, started from the app switcher and receives an intent in the foreground WHEN getting the start up path THEN it returns MAIN`() {
        every { intent.action } returns Intent.ACTION_MAIN
        launchApp(intent)
        stopLaunchedApp()
        startStoppedAppFromAppSwitcher()
        every { intent.action } returns Intent.ACTION_VIEW
        receiveIntentInForeground(intent)

        assertEquals(StartupPath.MAIN, provider.startupPathForActivity)
    }

    private fun launchApp(intent: Intent) {
        callbacks.onCreate(mockk())
        provider.onIntentReceived(intent)
        callbacks.onStart(mockk())
        callbacks.onResume(mockk())
    }

    private fun stopLaunchedApp() {
        callbacks.onPause(mockk())
        callbacks.onStop(mockk())
    }

    private fun startStoppedApp(intent: Intent) {
        callbacks.onStart(mockk())
        provider.onIntentReceived(intent)
        callbacks.onResume(mockk())
    }

    private fun startStoppedAppFromAppSwitcher() {
        // What makes the app switcher case special is it starts the app without an intent.
        callbacks.onStart(mockk())
        callbacks.onResume(mockk())
    }

    private fun receiveIntentInForeground(intent: Intent) {
        // To my surprise, the app is paused before receiving an intent on Pixel 2.
        callbacks.onPause(mockk())
        provider.onIntentReceived(intent)
        callbacks.onResume(mockk())
    }
}
