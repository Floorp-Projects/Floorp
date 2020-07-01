/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.tips

import io.mockk.mockk
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class TipManagerTest {
    @Test
    fun `test first with shouldDisplay`() {
        val shouldDisplayProvider = object : TipProvider {
            override val tip = mockk<Tip>()
            override val shouldDisplay = true
        }
        val shouldNotDisplayProvider = object : TipProvider {
            override val tip = mockk<Tip>()
            override val shouldDisplay = false
        }
        val manager = FenixTipManager(listOf(shouldNotDisplayProvider, shouldDisplayProvider))
        assertEquals(shouldDisplayProvider.tip, manager.getTip())
    }

    @Test
    fun `test first with shouldDisplay even if tip is null`() {
        val shouldDisplayProvider = object : TipProvider {
            override val tip: Tip? = null
            override val shouldDisplay = true
        }
        val shouldNotDisplayProvider = object : TipProvider {
            override val tip = mockk<Tip>()
            override val shouldDisplay = false
        }
        val manager = FenixTipManager(listOf(shouldNotDisplayProvider, shouldDisplayProvider))
        assertEquals(shouldDisplayProvider.tip, manager.getTip())
    }

    @Test
    fun `test returns null with empty list`() {
        val manager = FenixTipManager(emptyList())
        assertNull(manager.getTip())
    }
}
