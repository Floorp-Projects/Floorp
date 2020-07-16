/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.browsingmode

import io.mockk.MockKAnnotations
import io.mockk.impl.annotations.MockK
import io.mockk.verify
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test

class DefaultBrowsingModeManagerTest {

    @MockK(relaxed = true) lateinit var callback: (BrowsingMode) -> Unit
    lateinit var manager: BrowsingModeManager

    private val initMode = BrowsingMode.Normal

    @Before
    fun before() {
        MockKAnnotations.init(this)
        manager = DefaultBrowsingModeManager(initMode, callback)
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

        manager.mode = BrowsingMode.Normal
        assertEquals(BrowsingMode.Normal, manager.mode)
    }
}
