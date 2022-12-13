/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.telemetry

import android.content.Intent
import androidx.lifecycle.Lifecycle
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mozilla.focus.telemetry.startuptelemetry.StartupPathProvider
import org.mozilla.focus.telemetry.startuptelemetry.StartupPathProvider.StartupPath

class StartupPathProviderTest {

    private lateinit var provider: StartupPathProvider
    private lateinit var callbacks: StartupPathProvider.StartupPathLifecycleObserver

    private val intent: Intent = mock()

    @Before
    fun setUp() {
        provider = StartupPathProvider()
        callbacks = provider.getTestCallbacks()
    }

    @Test
    fun `WHEN attach is called THEN the provider is registered to the lifecycle`() {
        val lifecycle = mock<Lifecycle>()
        provider.attachOnActivityOnCreate(lifecycle, null)

        verify(lifecycle).addObserver(any())
    }

    @Test
    fun `WHEN calling attach THEN the intent is passed to on intent received`() {
        // With this test, we're basically saying, "attach..." does the same thing as
        // "onIntentReceived" so we don't need to duplicate all the tests we run for
        // "onIntentReceived".
        val spyProvider = spy(provider)
        spyProvider.attachOnActivityOnCreate(mock(), intent)

        verify(spyProvider).onIntentReceived(intent)
    }

    @Test
    fun `GIVEN no intent is received and the activity is not started WHEN getting the start up path THEN it is not set`() {
        assertEquals(StartupPath.NOT_SET, provider.startupPathForActivity)
    }

    @Test
    fun `GIVEN a main intent is received but the activity is not started yet WHEN getting the start up path THEN main is returned`() {
        doReturn(Intent.ACTION_MAIN).`when`(intent).action
        provider.onIntentReceived(intent)
        assertEquals(StartupPath.MAIN, provider.startupPathForActivity)
    }

    @Test
    fun `GIVEN a main intent is received and the app is started WHEN getting the start up path THEN it is main`() {
        doReturn(Intent.ACTION_MAIN).`when`(intent).action
        callbacks.onCreate(mock())
        provider.onIntentReceived(intent)
        callbacks.onStart(mock())

        assertEquals(StartupPath.MAIN, provider.startupPathForActivity)
    }

    @Test
    fun `GIVEN the app is launched from the homeScreen WHEN getting the start up path THEN it is main`() {
        // There's technically more to a homeScreen Intent but it's fine for now.
        doReturn(Intent.ACTION_MAIN).`when`(intent).action
        launchApp(intent)
        assertEquals(StartupPath.MAIN, provider.startupPathForActivity)
    }

    @Test
    fun `GIVEN the app is launched by app link WHEN getting the start up path THEN it is view`() {
        // There's technically more to a homeScreen Intent but it's fine for now.
        doReturn(Intent.ACTION_VIEW).`when`(intent).action

        launchApp(intent)

        assertEquals(StartupPath.VIEW, provider.startupPathForActivity)
    }

    @Test
    fun `GIVEN the app is launched by a send action WHEN getting the start up path THEN it is unknown`() {
        doReturn(Intent.ACTION_SEND).`when`(intent).action

        launchApp(intent)

        assertEquals(StartupPath.UNKNOWN, provider.startupPathForActivity)
    }

    @Test
    fun `GIVEN the app is launched by a null intent (is this possible) WHEN getting the start up path THEN it is not set`() {
        callbacks.onCreate(mock())
        provider.onIntentReceived(null)
        callbacks.onStart(mock())
        callbacks.onResume(mock())

        assertEquals(StartupPath.NOT_SET, provider.startupPathForActivity)
    }

    @Test
    fun `GIVEN the app is launched to the homeScreen and stopped WHEN getting the start up path THEN it is not set`() {
        doReturn(Intent.ACTION_MAIN).`when`(intent).action

        launchApp(intent)
        stopLaunchedApp()

        assertEquals(StartupPath.NOT_SET, provider.startupPathForActivity)
    }

    @Test
    fun `GIVEN the app is launched to the homeScreen, stopped, and relaunched warm from app link WHEN getting the start up path THEN it is view`() {
        doReturn(Intent.ACTION_MAIN).`when`(intent).action

        launchApp(intent)
        stopLaunchedApp()

        doReturn(Intent.ACTION_VIEW).`when`(intent).action

        startStoppedApp(intent)

        assertEquals(StartupPath.VIEW, provider.startupPathForActivity)
    }

    @Test
    fun `GIVEN the app is launched to the homeScreen, stopped, and relaunched warm from the app switcher WHEN getting the start up path THEN it is not set`() {
        doReturn(Intent.ACTION_MAIN).`when`(intent).action

        launchApp(intent)
        stopLaunchedApp()
        startStoppedAppFromAppSwitcher()

        assertEquals(StartupPath.NOT_SET, provider.startupPathForActivity)
    }

    @Test
    fun `GIVEN the app is launched to the homeScreen, paused, and resumed WHEN getting the start up path THEN it returns the initial intent value`() {
        doReturn(Intent.ACTION_MAIN).`when`(intent).action

        launchApp(intent)
        callbacks.onPause(mock())
        callbacks.onResume(mock())

        assertEquals(StartupPath.MAIN, provider.startupPathForActivity)
    }

    @Test
    fun `GIVEN the app is launched with an intent and receives an intent while the activity is foregrounded WHEN getting the start up path THEN it returns the initial intent value`() {
        doReturn(Intent.ACTION_MAIN).`when`(intent).action

        launchApp(intent)
        doReturn(Intent.ACTION_VIEW).`when`(intent).action

        receiveIntentInForeground(intent)

        assertEquals(StartupPath.MAIN, provider.startupPathForActivity)
    }

    @Test
    fun `GIVEN the app is launched, stopped, started from the app switcher and receives an intent in the foreground WHEN getting the start up path THEN it returns not set`() {
        doReturn(Intent.ACTION_MAIN).`when`(intent).action

        launchApp(intent)
        stopLaunchedApp()
        startStoppedAppFromAppSwitcher()
        doReturn(Intent.ACTION_VIEW).`when`(intent).action

        receiveIntentInForeground(intent)

        assertEquals(StartupPath.NOT_SET, provider.startupPathForActivity)
    }

    private fun launchApp(intent: Intent) {
        callbacks.onCreate(mock())
        provider.onIntentReceived(intent)
        callbacks.onStart(mock())
        callbacks.onResume(mock())
    }

    private fun stopLaunchedApp() {
        callbacks.onPause(mock())
        callbacks.onStop(mock())
    }

    private fun startStoppedApp(intent: Intent) {
        callbacks.onStart(mock())
        provider.onIntentReceived(intent)
        callbacks.onResume(mock())
    }

    private fun startStoppedAppFromAppSwitcher() {
        // What makes the app switcher case special is it starts the app without an intent.
        callbacks.onStart(mock())
        callbacks.onResume(mock())
    }

    private fun receiveIntentInForeground(intent: Intent) {
        // To my surprise, the app is paused before receiving an intent on Pixel 2.
        callbacks.onPause(mock())
        provider.onIntentReceived(intent)
        callbacks.onResume(mock())
    }
}
