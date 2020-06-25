/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.content.Context
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.spyk
import io.mockk.verify
import org.junit.Test
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.utils.Settings

internal class FirstSessionPingTest {

    @Test
    fun `checkAndSend() triggers the ping if it wasn't marked as triggered`() {
        val mockedContext: Context = mockk(relaxed = true)
        val mockedSettings: Settings = mockk(relaxed = true)
        mockkStatic("org.mozilla.fenix.ext.ContextKt")
        every { mockedContext.settings() } returns mockedSettings
        val mockAp = spyk(FirstSessionPing(mockedContext), recordPrivateCalls = true)
        every { mockAp.checkMetricsNotEmpty() } returns true
        every { mockAp.wasAlreadyTriggered() } returns false
        every { mockAp.markAsTriggered() } just Runs

        mockAp.checkAndSend()

        verify(exactly = 1) { mockAp.triggerPing() }
        // Marking the ping as triggered happens in a co-routine off the main thread,
        // so wait a bit for it.
        verify(timeout = 5000, exactly = 1) { mockAp.markAsTriggered() }
    }

    @Test
    fun `checkAndSend() doesn't trigger the ping again if it was marked as triggered`() {
        val mockAp = spyk(FirstSessionPing(mockk()), recordPrivateCalls = true)
        every { mockAp.wasAlreadyTriggered() } returns true

        mockAp.checkAndSend()

        verify(exactly = 0) { mockAp.triggerPing() }
    }
}
