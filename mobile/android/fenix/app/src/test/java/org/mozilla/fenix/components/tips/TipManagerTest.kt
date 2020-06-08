/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.tips

import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.unmockkObject
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.FeatureFlags

class TipManagerTest {

    @Before
    fun setup() {
        mockkObject(FeatureFlags)
        every { FeatureFlags.tips } returns true
    }

    @After
    fun after() {
        unmockkObject(FeatureFlags)
    }

    @Test
    fun `test feature flag off`() {
        every { FeatureFlags.tips } returns false
        assertNull(FenixTipManager(emptyList()).getTip())
        assertNull(FenixTipManager(listOf(
            object : TipProvider {
                override val tip = mockk<Tip>()
                override val shouldDisplay = true
            },
            object : TipProvider {
                override val tip = mockk<Tip>()
                override val shouldDisplay = false
            },
            object : TipProvider {
                override val tip: Tip? = null
                override val shouldDisplay = true
            },
            object : TipProvider {
                override val tip: Tip? = null
                override val shouldDisplay = false
            }
        )).getTip())
    }

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
