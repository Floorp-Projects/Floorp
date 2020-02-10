/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.browsingmode

import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

class DefaultBrowsingModeManagerTest {

    lateinit var manager: DefaultBrowsingModeManager

    private val initMode = BrowsingMode.Normal

    @Before
    fun before() {
        manager = DefaultBrowsingModeManager(initMode)
    }

    @Test
    fun `WHEN mode is updated THEN callback is invoked`() {
        val browsingModeListener: BrowsingModeListener = mock()
        manager.registerBrowsingModeListener(browsingModeListener)

        verify(browsingModeListener, times(0)).onBrowsingModeChange(any())

        manager.mode = BrowsingMode.Private
        manager.mode = BrowsingMode.Private
        manager.mode = BrowsingMode.Private

        verify(browsingModeListener, times(3)).onBrowsingModeChange(any())

        manager.mode = BrowsingMode.Normal
        manager.mode = BrowsingMode.Normal

        verify(browsingModeListener, times(5)).onBrowsingModeChange(any())
    }

    @Test
    fun `WHEN mode is updated THEN it should be returned from get`() {
        assertEquals(BrowsingMode.Normal, manager.mode)

        manager.mode = BrowsingMode.Private
        assertEquals(BrowsingMode.Private, manager.mode)

        manager.mode = BrowsingMode.Normal
        assertEquals(BrowsingMode.Normal, manager.mode)
    }
}
