/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.os.StrictMode
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.mockkStatic
import io.mockk.unmockkObject
import io.mockk.unmockkStatic
import io.mockk.verify
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.Config
import org.mozilla.fenix.ReleaseChannel
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class StrictModeTest {

    private lateinit var threadPolicy: StrictMode.ThreadPolicy
    private lateinit var functionBlock: () -> String

    @Before
    fun setup() {
        threadPolicy = StrictMode.ThreadPolicy.LAX
        functionBlock = mockk()
        mockkStatic(StrictMode::class)
        mockkObject(Config)

        every { StrictMode.setThreadPolicy(threadPolicy) } just Runs
        every { functionBlock() } returns "Hello world"
    }

    @After
    fun teardown() {
        unmockkStatic(StrictMode::class)
        unmockkObject(Config)
    }

    @Test
    fun `runs function block in release`() {
        every { Config.channel } returns ReleaseChannel.Release
        assertEquals("Hello world", threadPolicy.resetPoliciesAfter(functionBlock))
        verify(exactly = 0) { StrictMode.setThreadPolicy(any()) }
    }

    @Test
    fun `runs function block in debug`() {
        every { Config.channel } returns ReleaseChannel.Debug
        assertEquals("Hello world", threadPolicy.resetPoliciesAfter(functionBlock))
        verify { StrictMode.setThreadPolicy(threadPolicy) }
    }

    @Test
    fun `sets thread policy even if function throws`() {
        every { Config.channel } returns ReleaseChannel.Debug
        every { functionBlock() } throws IllegalStateException()
        var exception: IllegalStateException? = null

        try {
            threadPolicy.resetPoliciesAfter(functionBlock)
        } catch (e: IllegalStateException) {
            exception = e
        }

        verify { StrictMode.setThreadPolicy(threadPolicy) }
        assertNotNull(exception)
    }
}
