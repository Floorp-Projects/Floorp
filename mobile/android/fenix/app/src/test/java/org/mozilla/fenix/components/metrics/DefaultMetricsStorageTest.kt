/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import io.mockk.every
import io.mockk.mockk
import io.mockk.slot
import kotlinx.coroutines.test.StandardTestDispatcher
import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.utils.Settings

class DefaultMetricsStorageTest {

    private var checkDefaultBrowser = false
    private val doCheckDefaultBrowser = { checkDefaultBrowser }
    private var shouldSendGenerally = true
    private val doShouldSendGenerally = { shouldSendGenerally }

    private val settings = mockk<Settings>()

    private val dispatcher = StandardTestDispatcher()

    private lateinit var storage: DefaultMetricsStorage

    @Before
    fun setup() {
        checkDefaultBrowser = false
        shouldSendGenerally = true
        storage = DefaultMetricsStorage(mockk(), settings, doCheckDefaultBrowser, doShouldSendGenerally, dispatcher)
    }

    @Test
    fun `GIVEN that events should not be generally sent WHEN event would be tracked THEN it is not`() = runTest(dispatcher) {
        shouldSendGenerally = false
        checkDefaultBrowser = true
        every { settings.setAsDefaultGrowthSent } returns false

        val result = storage.shouldTrack(Event.GrowthData.SetAsDefault)

        assertFalse(result)
    }

    @Test
    fun `GIVEN set as default has not been sent and app is not default WHEN checked for sending THEN will not be sent`() = runTest(dispatcher) {
        every { settings.setAsDefaultGrowthSent } returns false
        checkDefaultBrowser = false

        val result = storage.shouldTrack(Event.GrowthData.SetAsDefault)

        assertFalse(result)
    }

    @Test
    fun `GIVEN set as default has not been sent and app is default WHEN checked for sending THEN will be sent`() = runTest(dispatcher) {
        every { settings.setAsDefaultGrowthSent } returns false
        checkDefaultBrowser = true

        val result = storage.shouldTrack(Event.GrowthData.SetAsDefault)

        assertTrue(result)
    }

    @Test
    fun `GIVEN set as default has been sent and app is default WHEN checked for sending THEN will be not sent`() = runTest(dispatcher) {
        every { settings.setAsDefaultGrowthSent } returns true
        checkDefaultBrowser = true

        val result = storage.shouldTrack(Event.GrowthData.SetAsDefault)

        assertFalse(result)
    }

    @Test
    fun `WHEN set as default updated THEN settings will be updated accordingly`() = runTest(dispatcher) {
        val updateSlot = slot<Boolean>()
        every { settings.setAsDefaultGrowthSent = capture(updateSlot) } returns Unit

        storage.updateSentState(Event.GrowthData.SetAsDefault)

        assertTrue(updateSlot.captured)
    }

    @Test
    fun `GIVEN that it has been less than 24 hours since last resumed sent WHEN checked for sending THEN will not be sent`() = runTest(dispatcher) {
        val currentTime = System.currentTimeMillis()
        every { settings.resumeGrowthLastSent } returns currentTime

        val result = storage.shouldTrack(Event.GrowthData.FirstAppOpenForDay)

        assertFalse(result)
    }

    @Test
    fun `GIVEN that it has been more than 24 hours since last resumed sent WHEN checked for sending THEN will be sent`() = runTest(dispatcher) {
        val currentTime = System.currentTimeMillis()
        every { settings.resumeGrowthLastSent } returns currentTime - 1000 * 60 * 60 * 24 * 2

        val result = storage.shouldTrack(Event.GrowthData.FirstAppOpenForDay)

        assertTrue(result)
    }

    @Test
    fun `WHEN last resumed state updated THEN settings updated accordingly`() = runTest(dispatcher) {
        val updateSlot = slot<Long>()
        every { settings.resumeGrowthLastSent } returns 0
        every { settings.resumeGrowthLastSent = capture(updateSlot) } returns Unit

        storage.updateSentState(Event.GrowthData.FirstAppOpenForDay)

        assertTrue(updateSlot.captured > 0)
    }
}
