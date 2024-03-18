/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.browsingmode

import io.mockk.MockKAnnotations
import io.mockk.Runs
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.just
import io.mockk.verify
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.MockkRetryTestRule
import org.mozilla.fenix.utils.Settings

class DefaultBrowsingModeManagerTest {

    @MockK lateinit var settings: Settings

    @MockK(relaxed = true)
    lateinit var callback: (BrowsingMode) -> Unit
    lateinit var manager: BrowsingModeManager

    private val initMode = BrowsingMode.Normal

    @get:Rule
    val mockkRule = MockkRetryTestRule()

    @Before
    fun before() {
        MockKAnnotations.init(this)

        manager = DefaultBrowsingModeManager(initMode, settings, callback)
        every { settings.lastKnownMode = any() } just Runs
    }

    @Test
    fun `WHEN mode is updated THEN callback is invoked`() {
        verify(exactly = 0) { callback.invoke(any()) }

        manager.mode = BrowsingMode.Private
        manager.mode = BrowsingMode.Private
        manager.mode = BrowsingMode.Private

        verify(exactly = 3) { callback.invoke(BrowsingMode.Private) }

        manager.mode = BrowsingMode.Normal
        manager.mode = BrowsingMode.Normal

        verify(exactly = 2) { callback.invoke(BrowsingMode.Normal) }
    }

    @Test
    fun `WHEN mode is updated THEN it should be returned from get`() {
        assertEquals(BrowsingMode.Normal, manager.mode)

        manager.mode = BrowsingMode.Private
        assertEquals(BrowsingMode.Private, manager.mode)
        verify { settings.lastKnownMode = BrowsingMode.Private }

        manager.mode = BrowsingMode.Normal
        assertEquals(BrowsingMode.Normal, manager.mode)
        verify { settings.lastKnownMode = BrowsingMode.Normal }
    }
}
