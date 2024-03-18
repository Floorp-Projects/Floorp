/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.os.StrictMode
import androidx.fragment.app.FragmentManager
import io.mockk.MockKAnnotations
import io.mockk.confirmVerified
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.slot
import io.mockk.unmockkStatic
import io.mockk.verify
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.fail
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.Config
import org.mozilla.fenix.ReleaseChannel
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class StrictModeManagerTest {

    private lateinit var debugManager: StrictModeManager
    private lateinit var releaseManager: StrictModeManager

    @MockK(relaxUnitFun = true)
    private lateinit var fragmentManager: FragmentManager

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        mockkStatic(StrictMode::class)

        val components: Components = mockk(relaxed = true)

        // These tests log a warning that mockk couldn't set the backing field of Config.channel
        // but it doesn't seem to impact their correctness so I'm ignoring it.
        val debugConfig: Config = mockk { every { channel } returns ReleaseChannel.Debug }
        debugManager = StrictModeManager(debugConfig, components)

        val releaseConfig: Config = mockk { every { channel } returns ReleaseChannel.Release }
        releaseManager = StrictModeManager(releaseConfig, components)
    }

    @After
    fun teardown() {
        unmockkStatic(StrictMode::class)
    }

    @Test
    fun `GIVEN we're in a release build WHEN we enable strict mode THEN we don't set policies`() {
        releaseManager.enableStrictMode(false)
        verify(exactly = 0) { StrictMode.setThreadPolicy(any()) }
        verify(exactly = 0) { StrictMode.setVmPolicy(any()) }
    }

    @Test
    fun `GIVEN we're in a debug build WHEN we enable strict mode THEN we set policies`() {
        debugManager.enableStrictMode(false)
        verify { StrictMode.setThreadPolicy(any()) }
        verify { StrictMode.setVmPolicy(any()) }
    }

    @Test
    fun `GIVEN we're in a debug build WHEN we attach a listener THEN we attach to the fragment lifecycle and detach when onFragmentResumed is called`() {
        val callbacks = slot<FragmentManager.FragmentLifecycleCallbacks>()

        debugManager.attachListenerToDisablePenaltyDeath(fragmentManager)
        verify { fragmentManager.registerFragmentLifecycleCallbacks(capture(callbacks), false) }
        confirmVerified(fragmentManager)

        callbacks.captured.onFragmentResumed(fragmentManager, mockk())
        verify { fragmentManager.unregisterFragmentLifecycleCallbacks(callbacks.captured) }
    }

    @Test
    fun `GIVEN we're in a release build WHEN resetAfter is called THEN we return the value from the function block`() {
        val expected = "Hello world"
        val actual = releaseManager.resetAfter(StrictMode.allowThreadDiskReads()) { expected }
        assertEquals(expected, actual)
    }

    @Test
    fun `GIVEN we're in a debug build WHEN resetAfter is called THEN we return the value from the function block`() {
        val expected = "Hello world"
        val actual = debugManager.resetAfter(StrictMode.allowThreadDiskReads()) { expected }
        assertEquals(expected, actual)
    }

    @Test
    fun `GIVEN we're in a release build WHEN resetAfter is called THEN the old policy is not set`() {
        releaseManager.resetAfter(StrictMode.allowThreadDiskReads()) { "" }
        verify(exactly = 0) { StrictMode.setThreadPolicy(any()) }
    }

    @Test
    fun `GIVEN we're in a debug build WHEN resetAfter is called THEN the old policy is set`() {
        val expectedPolicy = StrictMode.allowThreadDiskReads()
        debugManager.resetAfter(expectedPolicy) { "" }
        verify { StrictMode.setThreadPolicy(expectedPolicy) }
    }

    @Test
    fun `GIVEN we're in a debug build WHEN resetAfter is called and an exception is thrown from the function THEN the old policy is set`() {
        val expectedPolicy = StrictMode.allowThreadDiskReads()
        try {
            debugManager.resetAfter(expectedPolicy) {
                throw IllegalStateException()
            }

            @Suppress("UNREACHABLE_CODE")
            fail("Expected previous method to throw.")
        } catch (e: IllegalStateException) { /* Do nothing */ }

        verify { StrictMode.setThreadPolicy(expectedPolicy) }
    }

    @Test
    fun `GIVEN we're in debug mode WHEN we suppress StrictMode THEN the suppressed count increases`() {
        assertEquals(0, debugManager.suppressionCount.get())
        debugManager.resetAfter(StrictMode.allowThreadDiskReads()) { "" }
        assertEquals(1, debugManager.suppressionCount.get())
    }
}
