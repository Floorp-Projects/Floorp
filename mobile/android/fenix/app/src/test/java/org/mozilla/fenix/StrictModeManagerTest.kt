/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.os.StrictMode
import androidx.fragment.app.FragmentManager
import io.mockk.MockKAnnotations
import io.mockk.confirmVerified
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.mockkStatic
import io.mockk.slot
import io.mockk.unmockkObject
import io.mockk.unmockkStatic
import io.mockk.verify
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class StrictModeManagerTest {

    @MockK(relaxUnitFun = true) private lateinit var fragmentManager: FragmentManager

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        mockkStatic(StrictMode::class)
        mockkObject(Config)
    }

    @After
    fun teardown() {
        unmockkStatic(StrictMode::class)
        unmockkObject(Config)
    }

    @Test
    fun `test enableStrictMode in release`() {
        every { Config.channel } returns ReleaseChannel.Release
        StrictModeManager.enableStrictMode(false)

        verify(exactly = 0) { StrictMode.setThreadPolicy(any()) }
        verify(exactly = 0) { StrictMode.setVmPolicy(any()) }
    }

    @Test
    fun `test enableStrictMode in debug`() {
        every { Config.channel } returns ReleaseChannel.Debug
        StrictModeManager.enableStrictMode(false)

        verify { StrictMode.setThreadPolicy(any()) }
        verify { StrictMode.setVmPolicy(any()) }
    }

    @Test
    fun `test changeStrictModePolicies`() {
        val callbacks = slot<FragmentManager.FragmentLifecycleCallbacks>()

        StrictModeManager.changeStrictModePolicies(fragmentManager)
        verify { fragmentManager.registerFragmentLifecycleCallbacks(capture(callbacks), false) }
        confirmVerified(fragmentManager)

        callbacks.captured.onFragmentResumed(fragmentManager, mockk())
        verify { fragmentManager.unregisterFragmentLifecycleCallbacks(callbacks.captured) }
    }
}
